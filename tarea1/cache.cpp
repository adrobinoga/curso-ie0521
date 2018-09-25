/*
 * file: cache.cpp
 * author: Alexander Marin 2018
 * 
 * This program simulates a cache using the SRRIP replacement policy
 * the associativity, cache line size and cache size may be passed
 * from the command line (see readme.md).
 * 
 * Objects:
 * To simulate this cache several objects are used, at the top level
 * a cache objects takes instructions(read/store) and
 * executes/simulates them, this object uses a hashmap of Sets objects
 * where the keys are the index bits and the Sets objects have a vector
 * of cache lines, which are modeled using the Way class.
 */

// separation line for printing
#define SEP_TABLE "#########################################\n"

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <chrono>
#include <math.h>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace std::chrono;


/*
 * Extracts a sequence of bits from an integer number given a
 * superior and inferior limit.
 * 
 * @param[in] num	Integer number.
 * @param[in] h_lim	Superior bit index.
 * @param[in] l_lim	Inferior bit index.
 * @return int 		Number given by the sequence of bits.
 * 
 */
int bit_crop(int num, int h_lim, int l_lim)
{
	return (num%((unsigned long)1<<h_lim))>>l_lim;
}


/*
 * Class to model a cache using the SRRIP replacement policy.
 */
class CacheSRRIP
{
private:
	int cache_s;	// cache size in KB
	int cache_w;	// cache associativity
	int cache_b;	// cache block size in bytes
	int index_offset;	// offset of the index bits
	int tag_offset;	// offset of the tag bits
	int srrip_m;	// value of M for SRRIP policy
	int access_cnt;	// access counter
	int read_hit_cnt;	// read hit counter
	int store_hit_cnt;	// store hit counter
	int read_misses_cnt; // read misses counter
	int store_misses_cnt;	// store misses counter
	int dirty_evicts_cnt;	// dirty evictions counter

public:

	/*
	 * Class representing a cache line.
	 */
	class Way
	{
	private:
		int tag;	// tag bits of the current line
		int rrpv;	// RRPV value
		int dirty_bit;	// 
	public:

	/*
	 * Constructor for cache line objects.
	 * 
	 * @param[in] max_rrpv	Max value for the RRPV parameter.
	 */
	Way(int max_rrpv)
	{
		rrpv = max_rrpv;	// defaults to max value
		tag = -1;	// defaults tag to -1
		dirty_bit = 0;	// clean by default
	}

	/*
	 * Modifies the tag of the cache line.
	 * 
	 * @param[in] t	New tag to be set.
	 */
	void set_tag(int t)
	{
		tag = t;
	}

	/*
	 * Returns the tag of this cache line.
	 * 
	 * @return int	The tag of this cache line.
	 */
	int get_tag()
	{
		return tag;
	}

	/*
	 * Modifies the RRPV value of the this cache line.
	 * 
	 * @param[in] v	New value of RRPV.
	 */
	void set_rrpv(int v)
	{
		rrpv=v;
	}

	/*
	 * Returns the RRPV value of this cache line.
	 *
	 * @return int	The RRPV value of this cache line.
	 */
	int get_rrpv()
	{
		return rrpv;
	}

	/*
	 * Sets to 1 the dirty bit of this cache line.
	 */
	void set_dirty_bit()
	{
		dirty_bit = 1;
	}

	/*
	 * Clears the dirty bit of this cache line.
	 */
	void clear_dirty_bit()
	{
		dirty_bit = 0;
	}

	/*
	 * Returns the dirty bit of this cache line.
	 * 
	 * @param[in] int Dirty bit of this cache line (1/0).
	 */
	int get_dirty_bit()
	{
		return dirty_bit;
	}
};

	/*
	 * This class models the sets of the cache, each with N posible
	 * ways, the number of ways is set from command line.
	 */
	class Set
	{
	private:
		int s_size;	// set size (associativity)
		int srrip_m;	// value of M for SRRIP
		int max_rrpv;  // max value for rrpv value
		vector<Way> ways;	// ways of this set
		
	public:
		
		/*
		 * Inits instance of a cache Set.
		 * 
		 * @param[in] s	Number of ways of this set.
		 * @param[in] m	M value for SRRIP.
		 */
		Set(int s, int m)
		{
			s_size = s;
			srrip_m=m;
			// computes max value of RRPV
			max_rrpv = pow(2,srrip_m)-1;
			// creates vector with cache lines
			for (int k=0;k<s_size;k++)
			{
				ways.push_back(Way(max_rrpv));
			}
		}

		/*
		 * Returns the size of this cache set.
		 * 
		 * @returns int	Size of this cache.
		 */
		int get_size()
		{
			return s_size;
		}

		/*
		 * Searches for a given tag in the set ways.
		 * 
		 * @param[in] tag	Tag to search.
		 * @returns int	Returns 1 if finds the way, and 0 otherwise.
		 */
		int read_way(int tag)
		{
			for(int k=0; k<s_size; k++)
			{
				if(ways[k].get_tag() == tag)
				{
					ways[k].set_rrpv(0); // hit then rrpv=0
					return 1; // hit
				}
			}
			return 0; // miss
		}

		/*
		 * Evicts a line caused by a read request.
		 * 
		 * @param[in] tag New tag to set.
		 * @returns int Returns 1 if there is a dirty eviction,
		 * 						0 otherwise.
		 */
		int read_evict_way(int tag)
		{
			while(1)
			{
				for(int k=0; k<s_size; k++)
				{
					if(ways[k].get_rrpv() == max_rrpv)
					{
						// hit then rrpv=max-1
						ways[k].set_rrpv(max_rrpv-1); 
						// new tag
						ways[k].set_tag(tag); 
						
						if (ways[k].get_dirty_bit() == 1)
						{
							// clear dirty bit
							ways[k].clear_dirty_bit();
							return 1;
						}
						else
						{
							return 0;
						}
					}
				}
				// increment all RRPV
				inc_all_rrpv();
			}
		}

		/*
		 * Searches for a block in this set, given a write request.
		 * 
		 * @param[in] tag Tag to search.
		 */
		int write_way(int tag)
		{
			for(int k=0; k<s_size; k++)
			{
				if(ways[k].get_tag() == tag)
				{
					ways[k].set_rrpv(0); // hit then rrpv=0
					ways[k].set_dirty_bit();
					return 1; // hit
				}
			}
			return 0; // miss
		}

		/*
		 * Removes a cache line in this set, given a write request.
		 * 
		 * @param[in] tag New tag to set.
		 * @returns int Returns 1 if there is a dirty eviction,
		 * 						0 otherwise.
		 */
		int write_evict_way(int tag)
		{
		while(1)
		{
			for(int k=0; k<s_size; k++)
			{
				if(ways[k].get_rrpv() == max_rrpv)
				{
					// hit then rrpv=max-1
					ways[k].set_rrpv(max_rrpv-1); 
					// new tag
					ways[k].set_tag(tag); 
					if (ways[k].get_dirty_bit() == 1)
					{
						return 1;
					}
					else
					{
						// modified value
						ways[k].set_dirty_bit();
						return 0;
					}
				}
			}
			// increment al RRPV values
			inc_all_rrpv();
			}
		}

		/*
		 * Increments al RRPV values of the current cache set.
		 */
		void inc_all_rrpv()
		{
			for(int k=0; k<s_size; k++)
			{
				int old_rrpv = ways[k].get_rrpv();
				ways[k].set_rrpv(old_rrpv+1);
			}
		}
};

	// hash map with the cache sets
	unordered_map<int,Set> map_sets;

	/*
	 * Inits cache values.
	 * @param[in] s	Size of the cache in KB.
	 * @param[in] w	Numer of ways of the cache.
	 * @param[in] b	Block size of the cache.
	 */
	CacheSRRIP(int s,int w,int b)
	{
		cache_s = s;	// cache size in KB
		cache_w = w;	// associativity
		cache_b = b;	// block size in bytes
		access_cnt=0;	// inits access counter
		read_hit_cnt = 0;	// inits read hit counter
		store_hit_cnt = 0;	// inits store hit counter
		read_misses_cnt = 0;	// inits read misses counter
		store_misses_cnt = 0;	// inits store misses counter
		dirty_evicts_cnt = 0;	// inits dirty eviction counter
		
		// definition of M SRRIP param
		if(cache_w>2)
			srrip_m = 2;
		else
			srrip_m = 1;

		// calculate index offset
		index_offset = (int)round(log2(cache_b));

		// calculate tag offset
		tag_offset = index_offset + (int)(log2(cache_s*pow(2,10)/(cache_w*cache_b)));
	}


	/*
	 * Retuns the M value of the SRRIP cache.
	 * 
	 * @returns int M value for SRRIP.
	 */
	int get_srrip_m()
	{
	return srrip_m;
	}

	/*
	 * Process a load request.
	 * 
	 * @param input_index	Index bits.
	 * @param input_tag	Tag bits.
	 */
	void load(int input_index, int tag)
	{
		if (map_sets.at(input_index).read_way(tag))
		{
			// hit
			// inc hit counter
			read_hit_cnt++;
		}
		else
		{
			// miss
			// inc miss counter
			read_misses_cnt++;
			// check for dirty eviction
			if (map_sets.at(input_index).read_evict_way(tag))
			{
				dirty_evicts_cnt++;
			}
		}
	}	

	/*
	 * Process a store request.
	 * 
	 * @param input_index	Index bits.
	 * @param input_tag	Tag bits.
	 */
	void store(int input_index, int tag)
	{
		if (map_sets.at(input_index).write_way(tag))
		{
			// hit
			// inc hit counter
			store_hit_cnt++;
		}
		else
		{
			// miss
			// inc miss counter
			store_misses_cnt++;
			if (map_sets.at(input_index).write_evict_way(tag))
			{
				// inc dirty
				dirty_evicts_cnt++;
			}
		}
	}

	/*
	 * Processes a write/load request.
	 * 
	 * @param[in] ls Indicates type of request(1:store/0:load).
	 * @param[in] phy_addr Physical address to write/read.
	 */
	void run(int ls, int phy_addr)
	{
		access_cnt++;
		int input_index = 0;
		int input_tag = 0;
		
		// extract index
		input_index = bit_crop(phy_addr, tag_offset, index_offset);
		// extract tag
		input_tag = bit_crop(phy_addr, 32, tag_offset);
		
		// check for set existence
		if(map_sets.find(input_index) == map_sets.end())
		{
			// create set for that index
			map_sets.insert(make_pair(input_index, Set(cache_w,srrip_m)));
		}
		
		if (ls == 0)
		{
			// load value
			load(input_index, input_tag);
		}
		else
		{
			// write value
			store(input_index,input_tag);
		}
	}

	/*
	 * Returns access counter.
	 * 
	 * @returns int Access counter.
	 */
	int get_access_cnt()
	{
		return access_cnt;
	}

	/*
	 * Returns read hit counter.
	 * 
	 * @returns int Read hit counter.
	 */
	int get_read_hit_cnt()
	{
		return read_hit_cnt;
	}

	/*
	 * Returns the store hit counter.
	 * 
	 * @returns int Store hit counter.
	 */
	int get_store_hit_cnt()
	{
		return store_hit_cnt;
	}

	/*
	 * Returns the misses counter.
	 * 
	 * @returns int Misses counter.
	 */
	int get_read_misses_cnt()
	{
		return read_misses_cnt;
	}

	/*
	 * Returns the store misses counter.
	 * 
	 * @returns int Stores misses counter.
	 */
	int get_store_misses_cnt()
	{
		return store_misses_cnt;
	}
	
	/*
	 * Returns the dirty evictions counter.
	 * 
	 * @returns int Dirty evictions counter.
	 */
	int get_dirty_evicts_cnt()
	{
		return dirty_evicts_cnt;
	}
};


int main(int argc, char** argv)
{
	int cache_size = 0;
	int cache_ways = 0;
	int cache_block_size = 0;
	char c;
	// reads options from command line (all are required)
	while ((c = getopt (argc, argv, "t:a:l:")) != -1)
		switch (c)
		{
			case 't':
				cache_size = stoi(optarg);
				break;
			case 'a':
				cache_ways = stoi(optarg);
				break;
			case 'l':
				cache_block_size = stoi(optarg);
				break;
		}

	// start simulation timer
	auto start = high_resolution_clock::now();

	double miss_rate = 0.0;
	double read_miss_rate = 0.0;
	int dirty_evictions_cnt = 0;
	int load_misses_cnt = 0;
	int store_misses_cnt = 0;
	int total_misses_cnt = 0;
	int load_hits_cnt = 0;
	int store_hits_cnt = 0;
	int total_hits_cnt = 0;
	int access_cnt = 0;
	
	int ls;
	int phy_addr;
	
	// create cache instance
	CacheSRRIP ch(cache_size, cache_ways, cache_block_size);
	for (string line; getline(cin, line);)
	{
		// process trace line
		ls = (int)line[2]-48;
		phy_addr = stoi(line.substr(4,12), nullptr, 16);
		ch.run(ls, phy_addr);
	}
	
	store_hits_cnt = ch.get_store_hit_cnt();
	load_hits_cnt = ch.get_read_hit_cnt();
	store_misses_cnt = ch.get_store_misses_cnt();
	load_misses_cnt = ch.get_read_misses_cnt();
	dirty_evictions_cnt = ch.get_dirty_evicts_cnt();
	access_cnt = ch.get_access_cnt();
	total_hits_cnt = load_hits_cnt + store_hits_cnt;
	total_misses_cnt = load_misses_cnt + store_misses_cnt;
	
	// calculate params
	miss_rate = ((double)load_misses_cnt + store_misses_cnt)/access_cnt;
	read_miss_rate = (double)load_misses_cnt/access_cnt;
	
	// stop simulation timer
	auto stop = high_resolution_clock::now(); 

	auto duration = duration_cast<milliseconds>(stop-start).count();

	// print simulation params
	printf("\n");
	printf(SEP_TABLE);
	printf("# Cache parameters:\n");
	printf("%-30s%-10d\n", "Cache size (KB):", cache_size);
	printf("%-30s%-10d\n", "Cache associativity:", cache_ways);
	printf("%-30s%-10d\n", "Cache block size:", cache_block_size);
	printf("\n");

	// print simulation results
	printf(SEP_TABLE);
	printf("# Simulation results:\n");
	printf("%-30s%-10.4f\n", "Overall miss rate:", miss_rate);
	printf("%-30s%-10.4f\n", "Read miss rate:", read_miss_rate);
	printf("%-30s%-10d\n", "Dirty evictions:", dirty_evictions_cnt);
	printf("%-30s%-10d\n", "Load misses:", load_misses_cnt);
	printf("%-30s%-10d\n", "Store misses:", store_misses_cnt);
	printf("%-30s%-10d\n", "Total misses:", total_misses_cnt);
	printf("%-30s%-10d\n", "Loads hits:", load_hits_cnt);
	printf("%-30s%-10d\n", "Store hits:", store_hits_cnt);
	printf("%-30s%-10d\n", "Total hits:", total_hits_cnt);
	printf("\n");

	// print simulation execution data
	printf(SEP_TABLE);
	printf("%-30s%-10ld\n", "Simulation time (ms):", duration);
	printf("\n");

	return 0;
}
