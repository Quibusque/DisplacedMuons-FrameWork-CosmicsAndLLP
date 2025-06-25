import FWCore.ParameterSet.Config as cms

ntuples = cms.EDAnalyzer(
    "my_ntuplizer",
    nameOfOutput=cms.string("CosmicsMC_Ntuples.root"),
    isCosmics=cms.bool(False),
    isAOD=cms.bool(False),
    EventInfo=cms.InputTag("generator"),
    RunInfo=cms.InputTag("generator"),
    BeamSpot=cms.InputTag("offlineBeamSpot"),
    displacedMuonCollection=cms.InputTag("slimmedDisplacedMuons"),
    prunedGenParticles=cms.InputTag("prunedGenParticles"),
    bits=cms.InputTag("TriggerResults", "", "HLT"),
)
