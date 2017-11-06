#ifndef PARSE_HELPER_HPP
#define PARSE_HELPER_HPP

#include "base.hpp"

namespace Parse
{
	inline uint32_t lo(uint64_t arg) {return (uint32_t)arg;}
	inline uint32_t hi(uint64_t arg) {return (uint32_t)(arg >> 32);}
	
	namespace EventComparator
	{
		bool compare_time(event_t * event_1, event_t * event_2);
	}

	namespace LittleEndDecoder
	{
		void decode32(uint32_t word, char *info, int* index, char pad = '.'); 
		void decode64(uint64_t word, char *info, int *index, char pad = '.');
	}

	namespace QueueOps
	{
		uint64_t dequeue64(queue<uint64_t> &my_queue);
		void clear_queue(queue<uint64_t> &my_queue);
		void triple_enqueue(queue<uint64_t> &my_queue, uint64_t arg1, uint64_t arg2, uint64_t arg3);
		void queue2buffer(void *my_buffer, queue<uint64_t> &my_queue);
		string queue_to_string(queue<uint64_t> &my_queue);
	}
	
	map<uint64_t, list<event_t *>> parse(map<uint64_t, string> &files);
	map<uint64_t, list<event_t *>> divide_and_parse();
	list<event_t *> parse_backtrace();
}
#endif
