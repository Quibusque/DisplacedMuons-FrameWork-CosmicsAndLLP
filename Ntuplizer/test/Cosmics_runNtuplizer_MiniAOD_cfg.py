import FWCore.ParameterSet.Config as cms
import os
import argparse

#################################  CONSTANTS  #################################################
ERA = "C"
ReReco = True
###############################################################################################

# Argument parser
parser = argparse.ArgumentParser()
parser.add_argument(
    "-input",
    type=str,
    required=True,
    help="Either a .root file or a directory containing .root files to be processed.",
)
parser.add_argument(
    "-out_file", type=str, required=True, help="Output file name for the ntuples."
)
args = parser.parse_args()
main_dir = "/eos/home-m/mcrucian/datasets/"
single_file = True if args.input.endswith(".root") else False

process = cms.Process("demo")
process.load("Configuration.StandardSequences.GeometryDB_cff")
process.load("Configuration.StandardSequences.MagneticField_38T_cff")
process.load("Configuration.StandardSequences.FrontierConditions_GlobalTag_cff")
process.load("Configuration.StandardSequences.Reconstruction_cff")
process.load("Configuration.StandardSequences.Services_cff")

# Debug printout and summary.
process.load("FWCore.MessageService.MessageLogger_cfi")

process.options = cms.untracked.PSet(
    wantSummary=cms.untracked.bool(True),
    # Set up multi-threaded run. Must be consistent with config.JobType.numCores in crab_cfg.py.
    # numberOfThreads=cms.untracked.uint32(8)
)

from Configuration.AlCa.GlobalTag import GlobalTag

# Select number of events to be processed
nEvents = -1
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(nEvents))

# Read events
if single_file:
    # If a single file is provided, use it directly
    listOfFiles = ["file:" + os.path.join(main_dir, args.input)]
else:
    # If a directory is provided, list all .root files in it
    my_dir = os.path.join(main_dir, args.input)
    if not os.path.isdir(my_dir):
        raise ValueError(f"Provided input '{my_dir}' is not a valid directory.")

    # Collect all .root files in the specified directory
    listOfFiles = [
        os.path.join(my_dir, file)
        for file in os.listdir(my_dir)
        if file.endswith(".root")
    ]
    if not listOfFiles:
        raise ValueError(f"No .root files found in the directory '{my_dir}'.")

process.source = cms.Source(
    "PoolSource",
    fileNames=cms.untracked.vstring(listOfFiles),
    secondaryFileNames=cms.untracked.vstring(),
    skipEvents=cms.untracked.uint32(0),
)
if ERA in "ABCD" and not ReReco:
    gTag = "124X_dataRun3_PromptAnalysis_v1"
if ERA in "ABCDE" and ReReco:
    gTag = "124X_dataRun3_v15"
if ERA in "FG" and not ReReco:
    gTag = "124X_dataRun3_PromptAnalysis_v2"
process.GlobalTag = GlobalTag(process.GlobalTag, gTag)

## Define the process to run
##
process.load("DisplacedMuons-FrameWork.Ntuplizer.Cosmics_ntuples_MiniAOD_cfi")

process.ntuples.nameOfOutput = args.out_file

process.p = cms.EndPath(process.ntuples)
