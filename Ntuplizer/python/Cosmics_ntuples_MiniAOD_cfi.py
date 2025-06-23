import FWCore.ParameterSet.Config as cms

ntuples = cms.EDAnalyzer(
    "ntuplizer",
    nameOfOutput=cms.string("ntuples.root"),
    isCosmics=cms.bool(True),
    isAOD=cms.bool(False),
    EventInfo=cms.InputTag("generator"),
    RunInfo=cms.InputTag("generator"),
    BeamSpot=cms.InputTag("offlineBeamSpot"),
    displacedMuonCollection=cms.InputTag("slimmedDisplacedMuons"),
    bits=cms.InputTag("TriggerResults", "", "HLT"),
)
