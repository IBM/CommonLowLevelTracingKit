#!/usr/bin/python3
# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

# %%
import os, sys
sys.path.append(os.path.dirname(os.path.realpath(__file__)))

import unittest
from helper.build_temp_target import process, Language
import tempfile
import os
import tarfile

TRACEBUFFER_INFO_COUNT = 7

#%% test cases

class snapshot_tests(unittest.TestCase):

    def test_snapshot_with_info(self: unittest.TestCase):
        language = Language.CPP
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            #include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
            #include <iostream>
            #include <cassert>
            #include <cstddef>
            #include <cstdio>
            #include <fstream>
            #include <string>
            #include <tuple>
            #include <sstream>
            CLLTK_TRACEBUFFER(BUFFER, 4096);
            int main(int argc, char* argv[])
            {   
                CLLTK_TRACEPOINT(BUFFER, "%s", "before snapshot");
                
                assert(argc == 2);
                
                std::stringstream tar_content;
                auto func = [&tar_content](const void *data, size_t size) -> std::optional<size_t> {
                    tar_content.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
                    return size;
                };
                std::vector<std::string> tracepoints = {};
                tracepoints.push_back("additional infos");
                assert(CommonLowLevelTracingKit::snapshot::take_snapshot(func, tracepoints).has_value());
  

                auto tar_file_path = std::string(argv[1]) + "/trace.clltk_traces";
                std::ofstream tar_file(tar_file_path);
                assert(tar_file.is_open());
                tar_file << tar_content.rdbuf();
                
                CLLTK_TRACEPOINT(BUFFER, "%s", "after snapshot");
                return 0;
            }
            '''
        data = process(file_content, language=language)
        self.assertEqual(len(data), 11 + TRACEBUFFER_INFO_COUNT)
        self.assertEqual(len(data[data["formatted"] == "before snapshot"]), 2)
        self.assertEqual(len(data[data["formatted"] == "additional infos"]), 1)
        self.assertEqual(len(data[data["formatted"] == "after snapshot"]), 1)
        
        pass

    def test_snapshot_without_info(self: unittest.TestCase):
        language = Language.CPP
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            #include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
            #include <iostream>
            #include <cassert>
            #include <cstddef>
            #include <cstdio>
            #include <fstream>
            #include <string>
            #include <tuple>
            #include <sstream>
            CLLTK_TRACEBUFFER(BUFFER, 4096);
            int main(int argc, char* argv[])
            {   
                CLLTK_TRACEPOINT(BUFFER, "%s", "before snapshot");
                
                assert(argc == 2);
                
                std::stringstream tar_content;
                auto func = [&tar_content](const void *data, size_t size) -> std::optional<size_t> {
                    tar_content.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
                    return size;
                };
                assert(CommonLowLevelTracingKit::snapshot::take_snapshot(func).has_value());
  

                auto tar_file_path = std::string(argv[1]) + "/trace.clltk_traces";
                std::ofstream tar_file(tar_file_path);
                assert(tar_file.is_open());
                tar_file << tar_content.rdbuf();
                
                CLLTK_TRACEPOINT(BUFFER, "%s", "after snapshot");
                return 0;
            }
            '''
        def callback(tmp : tempfile.TemporaryDirectory):
            for root, dirs, dir_files in os.walk(tmp.name):
                for file in dir_files:
                    if file != "trace.clltk_traces":
                        continue
                    with tarfile.open(root+"/"+file) as archive:
                        for file_in_archive in archive:
                            if not file_in_archive.name.endswith((".clltk_trace", ".json")):
                                self.fail(f"unknown file {file_in_archive.name}")
                    
            return
        data = process(file_content, language=language, check_callback=callback)
        self.assertEqual(len(data[data["formatted"].str.contains('tracebuffer info path')]), 2)
        pass
    
    def test_snapshot_without_anything(self: unittest.TestCase):
        language = Language.CPP
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
            #include <iostream>
            #include <cassert>
            #include <cstddef>
            #include <cstdio>
            #include <fstream>
            #include <string>
            #include <tuple>
            #include <sstream>
            int main(int argc, char* argv[])
            {   
                std::cout << "argc = " << argc << std::endl;
                assert(argc == 2);
                std::stringstream tar_content;
                auto func = [&tar_content](const void *data, size_t size) -> std::optional<size_t> {
                    tar_content.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
                    return size;
                };
                assert(CommonLowLevelTracingKit::snapshot::take_snapshot(func).has_value());
  
                auto tar_file_path = std::string(argv[1]) + "/trace.clltk_traces";
                std::ofstream tar_file(tar_file_path);
                assert(tar_file.is_open());
                tar_file << tar_content.rdbuf();
                
                return 0;
            }
            '''

        def callback(tmp : tempfile.TemporaryDirectory):
            for root, dirs, dir_files in os.walk(tmp.name):
                for file in dir_files:
                    if file != "trace.clltk_traces":
                        continue
                    with tarfile.open(root+"/"+file) as archive:
                        for file_in_archive in archive:
                            if not file_in_archive.name.endswith((".clltk_trace", ".json")):
                                self.fail(f"unknown file {file_in_archive.name}")
                    
            return
        data = process(file_content, language=language, check_callback=callback)
        self.assertEqual(len(data[data["formatted"].str.contains('tracebuffer info path')]), 0)
        pass
    
    def test_snapshot_with_data_after(self: unittest.TestCase):
        language = Language.CPP
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            #include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
            #include <iostream>
            #include <cassert>
            #include <cstddef>
            #include <cstdio>
            #include <fstream>
            #include <string>
            #include <tuple>
            #include <sstream>
            CLLTK_TRACEBUFFER(BUFFER, 4096);
            int main(int argc, char* argv[])
            {   
                CLLTK_TRACEPOINT(BUFFER, "%s", "before snapshot");
                
                assert(argc == 2);
                
                std::stringstream tar_content;
                auto func = [&tar_content](const void *data, size_t size) -> std::optional<size_t> {
                    tar_content.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
                    return size;
                };
                std::vector<std::string> tracepoints = {};
                tracepoints.push_back("additional infos");
                assert(CommonLowLevelTracingKit::snapshot::take_snapshot(func, tracepoints).has_value());
                const std::string data = "dummy";
                func(data.data(), data.size());
  

                auto tar_file_path = std::string(argv[1]) + "/trace.clltk_traces";
                std::ofstream tar_file(tar_file_path);
                assert(tar_file.is_open());
                tar_file << tar_content.rdbuf();
                
                CLLTK_TRACEPOINT(BUFFER, "%s", "after snapshot");
                return 0;
            }
            '''
        data = process(file_content, language=language)
        self.assertEqual(len(data), 11 + TRACEBUFFER_INFO_COUNT)
        self.assertEqual(len(data[data["formatted"] == "before snapshot"]), 2)
        self.assertEqual(len(data[data["formatted"] == "additional infos"]), 1)
        self.assertEqual(len(data[data["formatted"] == "after snapshot"]), 1)
        
        pass
class snapshot_compressed_tests(unittest.TestCase):

    def test_snapshot_with_info(self: unittest.TestCase):
        language = Language.CPP
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            #include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
            #include <iostream>
            #include <cassert>
            #include <cstddef>
            #include <cstdio>
            #include <fstream>
            #include <string>
            #include <tuple>
            #include <sstream>
            CLLTK_TRACEBUFFER(BUFFER, 4096);
            int main(int argc, char* argv[])
            {   
                CLLTK_TRACEPOINT(BUFFER, "%s", "before snapshot");
                
                assert(argc == 2);
                
                std::stringstream tar_content;
                auto func = [&tar_content](const void *data, size_t size) -> std::optional<size_t> {
                    tar_content.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
                    return size;
                };
                
                std::vector<std::string> tracepoints = {};
                tracepoints.push_back("additional infos");
                assert(CommonLowLevelTracingKit::snapshot::take_snapshot(func, tracepoints).has_value());
  
  

                auto tar_file_path = std::string(argv[1]) + "/trace.clltk_traces";
                std::ofstream tar_file(tar_file_path);
                assert(tar_file.is_open());
                tar_file << tar_content.rdbuf();
                
                CLLTK_TRACEPOINT(BUFFER, "%s", "after snapshot");
                return 0;
            }
            '''

        data = process(file_content, language=language)
        self.assertEqual(len(data), 11 + TRACEBUFFER_INFO_COUNT)
        self.assertEqual(len(data[data["formatted"] == "before snapshot"]), 2)
        self.assertEqual(len(data[data["formatted"] == "additional infos"]), 1)
        self.assertEqual(len(data[data["formatted"] == "after snapshot"]), 1)
        pass

    def test_snapshot_without_info(self: unittest.TestCase):
        language = Language.CPP
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            #include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
            #include <iostream>
            #include <cassert>
            #include <cstddef>
            #include <cstdio>
            #include <fstream>
            #include <string>
            #include <tuple>
            #include <sstream>
            CLLTK_TRACEBUFFER(BUFFER, 1024*1024);
            int main(int argc, char* argv[])
            {   
                CLLTK_TRACEPOINT(BUFFER, "%s", "before snapshot");
                
                assert(argc == 2);
                
                std::stringstream tar_content;
                auto func = [&tar_content](const void *data, size_t size) -> std::optional<size_t> {
                    tar_content.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
                    return size;
                };
                assert(CommonLowLevelTracingKit::snapshot::take_snapshot_compressed(func).has_value());
  

                auto tar_file_path = std::string(argv[1]) + "/trace.clltk_traces";
                std::ofstream tar_file(tar_file_path);
                assert(tar_file.is_open());
                tar_file << tar_content.rdbuf();
                
                CLLTK_TRACEPOINT(BUFFER, "%s", "after snapshot");
                return 0;
            }
            '''
        data = process(file_content, language=language)
        self.assertEqual(len(data[data["formatted"].str.contains('tracebuffer info path')]),2 )
        pass
    
    def test_snapshot_without_anything(self: unittest.TestCase):
        language = Language.CPP
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
            #include <iostream>
            #include <cassert>
            #include <cstddef>
            #include <cstdio>
            #include <fstream>
            #include <string>
            #include <tuple>
            #include <sstream>
            int main(int argc, char* argv[])
            {   
                std::cout << "argc = " << argc << std::endl;
                assert(argc == 2);
                std::stringstream tar_content;
                auto func = [&tar_content](const void *data, size_t size) -> std::optional<size_t> {
                    tar_content.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
                    return size;
                };
                assert(CommonLowLevelTracingKit::snapshot::take_snapshot_compressed(func).has_value());
  
                auto tar_file_path = std::string(argv[1]) + "/trace.clltk_traces";
                std::ofstream tar_file(tar_file_path);
                assert(tar_file.is_open());
                tar_file << tar_content.rdbuf();
                
                return 0;
            }
            '''

        data = process(file_content, language=language)
        self.assertEqual(len(data[data["formatted"].str.contains('tracebuffer info path')]),0 )
        pass
    
    def test_snapshot_with_data_after(self: unittest.TestCase):
        language = Language.CPP
        file_content = \
            '''
            #include "CommonLowLevelTracingKit/tracing/tracing.h"
            #include "CommonLowLevelTracingKit/snapshot/snapshot.hpp"
            #include <iostream>
            #include <cassert>
            #include <cstddef>
            #include <cstdio>
            #include <fstream>
            #include <string>
            #include <tuple>
            #include <sstream>
            CLLTK_TRACEBUFFER(BUFFER, 4096);
            int main(int argc, char* argv[])
            {   
                CLLTK_TRACEPOINT(BUFFER, "%s", "before snapshot");
                
                assert(argc == 2);
                
                std::stringstream tar_content;
                auto func = [&tar_content](const void *data, size_t size) -> std::optional<size_t> {
                    tar_content.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
                    return size;
                };
                std::vector<std::string> tracepoints = {};
                tracepoints.push_back("additional infos");
                assert(CommonLowLevelTracingKit::snapshot::take_snapshot_compressed(func, tracepoints).has_value());
                const std::string data = "dummy";
                func(data.data(), data.size());
  

                auto tar_file_path = std::string(argv[1]) + "/trace.clltk_traces";
                std::ofstream tar_file(tar_file_path);
                assert(tar_file.is_open());
                tar_file << tar_content.rdbuf();
                
                CLLTK_TRACEPOINT(BUFFER, "%s", "after snapshot");
                return 0;
            }
            '''
        data = process(file_content, language=language)
        self.assertEqual(len(data), 11 + TRACEBUFFER_INFO_COUNT)
        self.assertEqual(len(data[data["formatted"] == "before snapshot"]), 2)
        self.assertEqual(len(data[data["formatted"] == "additional infos"]), 1)
        self.assertEqual(len(data[data["formatted"] == "after snapshot"]), 1)
        
        pass