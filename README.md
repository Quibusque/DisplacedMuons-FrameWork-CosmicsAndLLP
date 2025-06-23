# Displaced Muons Framework

Taken from [DisplacedMuons-FrameWork](https://github.com/24LopezR/DisplacedMuons-FrameWork)

## Instructions for Installing and Producing Cosmic Data/MC Plots

### Installing

I recommend using `CMSSW_13_2_0`, but any later release should also work. For installing the code, follow these instructions:

```bash
cmsrel CMSSW_13_2_0
cd CMSSW_13_2_0/src
cmsenv
git clone git@github.com:24LopezR/DisplacedMuons-FrameWork.git
scram b -j8
