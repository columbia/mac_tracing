#include "parser.hpp"

namespace Parse
{
	namespace EventComparator
	{
		bool compare_time(event_t *event_1, event_t *event_2)
		{
			double timestamp1 = event_1->get_abstime();
			double timestamp2 = event_2->get_abstime();
			if (timestamp1 - timestamp2 <= 10e-8
				&& timestamp1-timestamp2 >= -10e-8)
				return (event_1->get_tid() < event_2->get_tid());
			else 
				return (timestamp1 < timestamp2);
		}
	}

	namespace LittleEndDecoder
	{
		void decode32(uint32_t word, char *info, int* index, char pad)
		{
			char code[4];
			code[0] = (char)((word & 0xff000000) >> 24);
			code[1] = (char)((word & 0x00ff0000) >> 16);
			code[2] = (char)((word & 0x0000ff00) >> 8);
			code[3] = (char)(word & 0x000000ff);
			for (int i = 3; i >= 0; i--) {
				info[*index] = (code[i] >= 32 && code[i] < 127) ? code[i] : pad;
				*index = (*index) + 1;
			}
			info[*index] = '\0';
		}

		void decode64(uint64_t word, char *info, int *index, char pad) 
		{
			char code[8];
			code[0] = (char)(word & 0x00000000000000ff);
			code[1] = (char)((word & 0x000000000000ff00) >> 8);
			code[2] = (char)((word & 0x0000000000ff0000) >> 16);
			code[3] = (char)((word & 0x00000000ff000000) >> 24);
			code[4] = (char)((word & 0x000000ff00000000) >> 32);
			code[5] = (char)((word & 0x0000ff0000000000) >> 40);
			code[6] = (char)((word & 0x00ff000000000000) >> 48);
			code[7] = (char)((word & 0xff00000000000000) >> 56);
			for (int i = 0; i < 8; i++) {
				info[*index] = (code[i] >= 32 && code[i] < 127) ? code[i] : pad;
				*index = (*index) + 1;	
			}
			info[*index] = '\0';
		}
	}

	namespace QueueOps
	{
		uint64_t dequeue64(queue<uint64_t> &q)
		{
			assert(q.size() > 0);
			uint64_t elem = q.front();
			q.pop();
			return elem;
		}

		void clear_queue(queue<uint64_t> &q)
		{
			queue<uint64_t> drain;
			swap(q, drain);
		}
	
		void triple_enqueue(queue<uint64_t> &q, uint64_t arg1, uint64_t arg2,
			uint64_t arg3)
		{
			q.push(arg1);
			q.push(arg2);
			q.push(arg3);
		}
		
		void queue2buffer(void *my_buffer, queue<uint64_t> &my_queue)
		{
			uint32_t i = 0; 
			uint64_t *array = (uint64_t *)my_buffer;
			while (!my_queue.empty())
				array[i++]= dequeue64(my_queue);	
		}
	
		string queue_to_string(queue<uint64_t> &q)
		{
			const int size = q.size();
			string ret = "";
			for (int i = 0; i < size; i++) {
				uint64_t elem = dequeue64(q);
				int index = 0;
				char tmp[16] = {0};
				LittleEndDecoder::decode64(elem, tmp, &index, '%');
				while(index--) {
					if (tmp[index] != '%')
						break;
				}
				ret.append(tmp, index + 1);
			}
			return ret;
		}
	}
}
