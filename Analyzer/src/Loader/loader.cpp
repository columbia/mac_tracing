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

	bool find_proc_in_procs(string proc_name)
	{
		ifstream infile(meta_data.procs_file);	
		if (infile.fail()) {
			cerr << "Error: fail to open file " << meta_data.procs_file << endl;
			exit(EXIT_FAILURE);
		}
		string line, procname;
		pid_t pid = 0;
		while (getline(infile, line)) {
			istringstream iss(line);
			if (!(iss >> pid))
				goto out;
			if (!getline(iss >> ws, procname) || !procname.size())
				goto out;
			if (procname == proc_name) {
				infile.close();
				return true;
			}
		}
	out:
		infile.close();
		cerr << "Fail to read procname " << proc_name << " from " << meta_data.procs_file << endl;
		return false;
	}

#include <dirent.h>
	bool load_lib(string proc_name) 
	{
		if (!meta_data.libs_dir.size())
			return false;
		DIR *dir;
		struct dirent *entry;
		dir = opendir(meta_data.libs_dir.c_str());

		if (!dir) {
			cerr << "Error: missing libraries info directory" << endl;
			return false;
		}

		while ((entry = readdir(dir))) {
			if (!(entry->d_name)) {
				cerr << "Error: fail to read file name from entry " << endl;
				goto out;
			}

			if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
				continue;
			string filename(meta_data.libs_dir + "/" + entry->d_name);
			ifstream infile(filename);
			if (infile.fail()) {
				cerr << "Error: fail to open file " << entry->d_name << endl;
				goto out;
			}

			{
				string line;
				if (!getline(infile, line)) {
					infile.close();
					cerr << "fail 0" << endl;
					goto out;
				}

				istringstream iss(line);
				string procname, arch, unused;
				pid_t pid;
				if (!(iss >> unused >> hex >> pid >> arch) || !(getline(iss >> ws, procname)) || !procname.size()) {
					infile.close();
					cerr << "fail 1" << endl;
					goto out;
				}

				if (procname == proc_name) {
					//append file to lib file
					ofstream outfile(meta_data.libinfo_file, ios::out | ios::app);
					infile.seekg(0);
					outfile << infile.rdbuf();
					outfile.close();
					infile.close();
					closedir(dir);
					return true;
				}
			}
		}
	out:
		closedir(dir);
		cerr << "Error: missing memory layout info for proc" << proc_name << endl;
		return false;
	}
	
	bool load_all_libs(void)
	{
		if (!meta_data.libs_dir.size() || !meta_data.procs_file.size())
			return false;
		ifstream infile(meta_data.procs_file);	
		if (infile.fail()) {
			cerr << "Error: fail to open file " << meta_data.procs_file << endl;
			exit(EXIT_FAILURE);
		}
		string line, procname;
		pid_t pid;
		while (getline(infile, line)) {
			istringstream iss(line);
			if (!(iss >> pid) || !getline(iss >> ws, procname) || !procname.size())
				goto out;

			if (!load_lib(procname)) {
				cerr << "Fail to load lib for proc " << procname  << "[" << pid << "]" << endl;
				continue;
			}
		}
	out:
		infile.close();
		return false;
	}
}
