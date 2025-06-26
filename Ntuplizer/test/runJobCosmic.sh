#!/bin/bash
# Make sure the script is executable: chmod +x runJob.sh
cd /afs/cern.ch/user/m/mcrucian/private/displaced_muons/CMSSW_13_3_0/src/DisplacedMuons-FrameWork-CosmicsAndLLP/Ntuplizer/test
cmsenv

# Get command line arguments: input directory and output file name
input=$1
out_file=$2

# Define log file
input_name=$(basename ${input%.*})
logfile="log_LLP_${input_name}_MiniAOD.log"

echo "Running cmsRun for input: ${input}"
echo "Output file: ${out_file}"
echo "Log file: ${logfile}"
start_time=$(date +%s)

cmsRun Cosmics_runNtuplizer_AOD_cfg.py -input ${input} -out_file ${out_file} &> ${logfile}

end_time=$(date +%s)
echo "Time taken: $((end_time - start_time)) seconds"

# Copy the output file (if it exists) to the EOS output directory 
output_dir="/eos/home-m/mcrucian/SWAN_projects/DisplacedMuons-CosmicsAndLLP/ntuples"
if [ -f "${out_file}" ]; then
    cp ${out_file} ${output_dir}
    echo "${out_file} copied to ${output_dir}"
else
    echo "Error: ${out_file} not found."
fi

