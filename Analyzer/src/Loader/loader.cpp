#include "loader.hpp"
namespace LoadData
{
	meta_data_t meta_data;
	map<uint64_t, pair<pid_t, string> > tpc_maps;
	map<int, string> mig_dictionary;
	map<string, uint64_t> bsc_name_index_map;
	map<string, uint64_t> msc_name_index_map;
	
	void load_tpc_maps(const string log_file, map<uint64_t, pair<pid_t, string> > &tpc_maps)
	{
		ifstream infile(log_file);
		if (infile.fail())
			return;
		string line;
		uint64_t tid;
		pid_t pid;
		string command;
		while (getline(infile, line)) {
			istringstream iss(line);
			if (!(iss >> hex >> tid >> pid))
				break;
			if (!getline(iss >> ws, command) || !command.size())
				break;
			tpc_maps[tid] = make_pair(pid, command);
		}
		
		map<uint64_t, pair<pid_t, string> >::iterator it;
		for (it = tpc_maps.begin(); it != tpc_maps.end(); it++) {
			tid = it->first;
			pid = (it->second).first;
			command = (it->second).second;
			//cout << "tid " << hex << tid\
			//<< "\tpid " << hex << pid\
			//<<"\t" << command << endl;
		}

		infile.close();
	}

	void load_mig_dictionary(const struct mig_service table[], uint32_t size, map<int, string> &mig_dictionary)
	{
		for (int i = 0; i < size; i++) {
			string name(table[i].mig_name);
			mig_dictionary[table[i].mig_num] = name;
		}
	}
	
	void build_syscall_name_index_map(const struct syscall_entry table[], uint32_t size, map<string, uint64_t> &sysmap)
	{
		sysmap.clear();
		for (int i = 0; i < size; i++) {
			string name(table[i].syscall_name);
			if (name.length() > 24) 
				name = name.substr(0, 24);
			sysmap[name] = i;
		}
	}

	void preload()
	{
		load_tpc_maps(meta_data.tpc_maps_file, tpc_maps);
		load_mig_dictionary(mig_table, mig_size, mig_dictionary);
		build_syscall_name_index_map(bsd_syscall_table, bsc_size, bsc_name_index_map);
		build_syscall_name_index_map(mach_syscall_table, msc_size, msc_name_index_map);
	}
		
	#define HASHVAL 17
	static inline uint64_t hash_tid(uint64_t tid, string unused) { return tid % HASHVAL + 1;}

	uint64_t map_op_code(uint64_t unused, string opname)
	{
		uint64_t code = 0;
		if (opname.find("BSC_") != string::npos)
			code = LoadData::op_code_map.at("BSD_SYSCALL");
		else if (opname.find("MSC_") != string::npos)
			code = LoadData::op_code_map.at("MACH_SYSCALL");
		else if (LoadData::op_code_map.find(opname) != op_code_map.end())
			code = LoadData::op_code_map.at(opname);
		return code;
	}
}
