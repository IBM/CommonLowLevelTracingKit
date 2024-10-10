# Copyright (c) 2024, International Business Machines
# SPDX-License-Identifier: BSD-2-Clause-Patent

#
# %%

# %%

import subprocess
import os
import tempfile
import glob

tmp_dir = tempfile.TemporaryDirectory()
# Create a dictionary to store the environment variables
env_vars = {"CLLTK_TRACING_PATH": tmp_dir.name}

# Define the command to run
currend_dir = os.path.dirname(os.path.realpath(__file__))
git_root = bin_path = os.path.join(currend_dir,'./../../')

example_name = 'extreme_c'
bin_path = glob.glob('./build/examples/**/example-' + example_name, recursive=True, root_dir=git_root)
assert len(bin_path) == 1
bin_path = bin_path[0]
bin_path = os.path.join(git_root, bin_path)

bin_arg = "100"

try:
    # Run the program with the specified arguments and environment variables
    result = subprocess.run([bin_path, bin_arg], env=env_vars, check=True)
    print(f"Program {bin_path} completed with return code {result.returncode}")
except subprocess.CalledProcessError as e:
    print(f"Error running {bin_path}: {e}")
except FileNotFoundError:
    print(f"Error: {bin_path} not found.")
except Exception as e:
    print(f"An unexpected error occurred: {e}")



decoder_path = os.path.join(git_root,'./decoder_tool/python/clltk_decoder.py')
decoder_path = os.path.realpath(decoder_path)
assert os.path.isfile(decoder_path), decoder_path

output_file_path = os.path.join(tmp_dir.name, 'output.csv')
print(f'{output_file_path = }')
decoder_arg = ['-o', output_file_path, tmp_dir.name]
try:
    # Run the program with the specified arguments and environment variables
    result = subprocess.run([decoder_path, *decoder_arg], env=env_vars, check=True)
    print(f"Program {decoder_path} completed with return code {result.returncode}")
except subprocess.CalledProcessError as e:
    print(f"Error running {decoder_path}: {e}")
except FileNotFoundError:
    print(f"Error: {decoder_path} not found.")
except Exception as e:
    print(f"An unexpected error occurred: {e}")

import pandas as pd

data = pd.read_csv(output_file_path)

# %
import numpy as np
import matplotlib.pyplot as plt


times = data['timestamp'].to_numpy(dtype="float64")
times.sort()
delta_times = np.diff(times)
delta_times *= 1e6


limit = 100
_delta_times = delta_times[delta_times < limit]

plt.figure(figsize=(10, 6))
plt.hist(_delta_times, bins=100, histtype="stepfilled", label="new")
plt.grid(True, "both")
plt.xlim([0, limit])
plt.yscale('log')
plt.xlabel("time delta [us]")
plt.ylabel("incidences")
plt.title(f'!with pthread_mutex! time between tracepoint timestamps of {example_name} with limit {limit}us mean {np.mean(_delta_times)}')
# %%
