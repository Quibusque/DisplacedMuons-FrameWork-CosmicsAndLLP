import FWCore.ParameterSet.Config as cms

ntuples = cms.EDAnalyzer(
    "my_ntuplizer",
    nameOfOutput=cms.string("CosmicsMC_Ntuples.root"),
    isCosmics=cms.bool(True),
    isAOD=cms.bool(True),
    EventInfo=cms.InputTag("generator"),
    RunInfo=cms.InputTag("generator"),
    BeamSpot=cms.InputTag("offlineBeamSpot"),
    displacedMuonCollection=cms.InputTag("displacedMuons"),
    bits=cms.InputTag("TriggerResults", "", "HLT"),
)
