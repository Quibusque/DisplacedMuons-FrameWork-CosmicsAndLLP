#include <memory>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
// #include "FWCore/Framework/interface/EDProducer.h"
#include <Math/Vector3D.h>
#include <Math/VectorUtil.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "DataFormats/Candidate/interface/Candidate.h"
#include "DataFormats/Common/interface/Handle.h"
#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/MuonReco/interface/Muon.h"
#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/PatCandidates/interface/PackedTriggerPrescales.h"
#include "DataFormats/PatCandidates/interface/TriggerObjectStandAlone.h"
#include "DataFormats/RecoCandidate/interface/RecoCandidate.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/VertexReco/interface/Vertex.h"
#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "SimDataFormats/GeneratorProducts/interface/HepMCProduct.h"
#include "TFile.h"
#include "TH1F.h"
#include "TLorentzVector.h"
#include "TTree.h"

namespace MTYPE {
const char* DSA = "DSA";
const char* DGL = "DGL";
}  // namespace MTYPE

typedef ROOT::Math::XYZVector XYZVector;
using ROOT::Math::VectorUtil::Angle;

float dxy_value(const reco::GenParticle& p, const reco::Vertex& pv) {
    float vx = p.vx();
    float vy = p.vy();
    float phi = p.phi();
    float pv_x = pv.x();
    float pv_y = pv.y();

    float dxy = -(vx - pv_x) * sin(phi) + (vy - pv_y) * cos(phi);
    return dxy;
}

bool passTagID(const reco::Track* track, const char* mtype) {
    bool passID = false;
    if (mtype == MTYPE::DSA) {
        if (track->phi() >= -0.8 || track->phi() <= -2.1) { return passID; }
        if (abs(track->eta()) >= 0.7) { return passID; }
        if (track->pt() <= 12.5) { return passID; }
        if (track->ptError() / track->pt() >= 0.2) { return passID; }
        if (track->hitPattern().numberOfValidMuonDTHits() <= 30) { return passID; }
        if (track->normalizedChi2() >= 2) { return passID; }
        passID = true;
    } else if (mtype == MTYPE::DGL) {
        if (track->phi() >= -0.6 || track->phi() <= -2.6) { return passID; }
        if (abs(track->eta()) >= 0.9) { return passID; }
        if (track->pt() <= 20) { return passID; }
        if (track->ptError() / track->pt() >= 0.3) { return passID; }
        if (track->hitPattern().numberOfMuonHits() <= 12) { return passID; }
        if (track->hitPattern().numberOfValidStripHits() <= 5) { return passID; }
        passID = true;
    } else {
        std::cout << "Error (in passTagID): wrong muon type" << std::endl;
    }
    return passID;
}

bool passProbeID(const reco::Track* track, const XYZVector& v_tag, const char* mtype) {
    bool passID = false;
    if (mtype == MTYPE::DSA) {
        if (track->hitPattern().numberOfValidMuonDTHits() +
                track->hitPattern().numberOfValidMuonCSCHits() <=
            12) {
            return passID;
        }
        if (track->pt() <= 3.5) { return passID; }
        XYZVector v_probe = XYZVector(track->px(), track->py(), track->pz());
        if (Angle(v_probe, v_tag) <= 2.1) { return passID; }
        passID = true;
    } else if (mtype == MTYPE::DGL) {
        if (track->pt() <= 20) { return passID; }
        XYZVector v_probe = XYZVector(track->px(), track->py(), track->pz());
        if (Angle(v_probe, v_tag) <= 2.8) { return passID; }
        passID = true;
    } else {
        std::cout << "Error (in passProbeID): wrong muon type" << std::endl;
    }
    return passID;
}

bool hasMotherWithPdgId(const reco::Candidate* particle, int pdgId) {
    // Loop on mothers, if any, and return true if a mother with the specified pdgId is found
    for (size_t i = 0; i < particle->numberOfMothers(); i++) {
        const reco::Candidate* mother = particle->mother(i);
        if (mother->pdgId() == pdgId || hasMotherWithPdgId(mother, pdgId)) { return true; }
    }
    return false;
}

class my_ntuplizer : public edm::one::EDAnalyzer<edm::one::SharedResources> {
   public:
    explicit my_ntuplizer(const edm::ParameterSet&);
    ~my_ntuplizer();

    edm::ConsumesCollector iC = consumesCollector();
    static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

   private:
    virtual void beginJob() override;
    virtual void analyze(const edm::Event&, const edm::EventSetup&) override;
    virtual void endJob() override;

    edm::ParameterSet parameters;

    bool isCosmics = true;
    bool isAOD = false;
    //
    // --- Tokens and Handles
    //

    // trigger bits
    edm::EDGetTokenT<edm::TriggerResults> triggerBits_;
    edm::Handle<edm::TriggerResults> triggerBits;

    // displacedMuons (reco::Muon // pat::Muon)
    edm::EDGetTokenT<edm::View<reco::Muon>> dmuToken;
    edm::Handle<edm::View<reco::Muon>> dmuons;
    // prunedGenParticles (reco::GenParticle)
    edm::EDGetTokenT<edm::View<reco::GenParticle>> prunedGenToken;
    edm::Handle<edm::View<reco::GenParticle>> prunedGen;

    // Trigger tags
    std::vector<std::string> HLTPaths_;
    bool triggerPass[200] = {false};

    // Event
    Int_t event = 0;
    Int_t lumiBlock = 0;
    Int_t run = 0;

    // ----------------------------------
    // displacedMuons
    // ----------------------------------
    Int_t ndmu = 0;
    Int_t dmu_isDSA[200] = {0};
    Int_t dmu_isDGL[200] = {0};
    Int_t dmu_isDTK[200] = {0};
    Int_t dmu_isMatchesValid[200] = {0};
    Int_t dmu_numberOfMatches[200] = {0};
    Int_t dmu_numberOfChambers[200] = {0};
    Int_t dmu_numberOfChambersCSCorDT[200] = {0};
    Int_t dmu_numberOfMatchedStations[200] = {0};
    Int_t dmu_numberOfMatchedRPCLayers[200] = {0};

    Float_t dmu_dsa_pt[200] = {0.};
    Float_t dmu_dsa_eta[200] = {0.};
    Float_t dmu_dsa_phi[200] = {0.};
    Float_t dmu_dsa_ptError[200] = {0.};
    Float_t dmu_dsa_dxy[200] = {0.};
    Float_t dmu_dsa_dz[200] = {0.};
    Float_t dmu_dsa_normalizedChi2[200] = {0.};
    Float_t dmu_dsa_charge[200] = {0.};
    Int_t dmu_dsa_nMuonHits[200] = {0};
    Int_t dmu_dsa_nValidMuonHits[200] = {0};
    Int_t dmu_dsa_nValidMuonDTHits[200] = {0};
    Int_t dmu_dsa_nValidMuonCSCHits[200] = {0};
    Int_t dmu_dsa_nValidMuonRPCHits[200] = {0};
    Int_t dmu_dsa_nValidStripHits[200] = {0};
    Int_t dmu_dsa_nhits[200] = {0};
    Int_t dmu_dsa_dtStationsWithValidHits[200] = {0};
    Int_t dmu_dsa_cscStationsWithValidHits[200] = {0};
    Int_t dmu_dsa_nsegments[200] = {0};
    // Variables for tag and probe
    bool dmu_dsa_passTagID[200] = {false};
    bool dmu_dsa_hasProbe[200] = {false};
    Int_t dmu_dsa_probeID[200] = {0};
    Float_t dmu_dsa_cosAlpha[200] = {0.};

    Float_t dmu_dgl_pt[200] = {0.};
    Float_t dmu_dgl_eta[200] = {0.};
    Float_t dmu_dgl_phi[200] = {0.};
    Float_t dmu_dgl_ptError[200] = {0.};
    Float_t dmu_dgl_dxy[200] = {0.};
    Float_t dmu_dgl_dz[200] = {0.};
    Float_t dmu_dgl_normalizedChi2[200] = {0.};
    Float_t dmu_dgl_charge[200] = {0.};
    Int_t dmu_dgl_nMuonHits[200] = {0};
    Int_t dmu_dgl_nValidMuonHits[200] = {0};
    Int_t dmu_dgl_nValidMuonDTHits[200] = {0};
    Int_t dmu_dgl_nValidMuonCSCHits[200] = {0};
    Int_t dmu_dgl_nValidMuonRPCHits[200] = {0};
    Int_t dmu_dgl_nValidStripHits[200] = {0};
    Int_t dmu_dgl_nhits[200] = {0};
    // Variables for tag and probe
    bool dmu_dgl_passTagID[200] = {false};
    bool dmu_dgl_hasProbe[200] = {false};
    Int_t dmu_dgl_probeID[200] = {0};
    Float_t dmu_dgl_cosAlpha[200] = {0.};

    Float_t dmu_dtk_pt[200] = {0.};
    Float_t dmu_dtk_eta[200] = {0.};
    Float_t dmu_dtk_phi[200] = {0.};
    Float_t dmu_dtk_ptError[200] = {0.};
    Float_t dmu_dtk_dxy[200] = {0.};
    Float_t dmu_dtk_dz[200] = {0.};
    Float_t dmu_dtk_normalizedChi2[200] = {0.};
    Float_t dmu_dtk_charge[200] = {0.};
    Int_t dmu_dtk_nMuonHits[200] = {0};
    Int_t dmu_dtk_nValidMuonHits[200] = {0};
    Int_t dmu_dtk_nValidMuonDTHits[200] = {0};
    Int_t dmu_dtk_nValidMuonCSCHits[200] = {0};
    Int_t dmu_dtk_nValidMuonRPCHits[200] = {0};
    Int_t dmu_dtk_nValidStripHits[200] = {0};
    Int_t dmu_dtk_nhits[200] = {0};

    // ----------------------------------
    // additional variables by Marco
    // ----------------------------------
    Float_t dmu_t0_InOut[200] = {0.};
    Float_t dmu_t0_OutIn[200] = {0.};
    bool dmu_dsa_isProbe[200] = {false};
    bool dmu_dgl_isProbe[200] = {false};
    // LLP gen matching
    bool dmu_dsa_genMatched[200] = {false};
    bool dmu_dgl_genMatched[200] = {false};
    Int_t dmu_dsa_genMatchingMultiplicity[200] = {0};
    Int_t dmu_dgl_genMatchingMultiplicity[200] = {0};
    Float_t dmu_dsa_genMatchingDeltaR[200] = {0.};
    Float_t dmu_dgl_genMatchingDeltaR[200] = {0.};
    Int_t dmu_dsa_genMatchedID[200] = {0};
    Int_t dmu_dgl_genMatchedID[200] = {0};
    Int_t ngenmu = 0;
    bool genmu_genMatched[200] = {false};
    Float_t genmu_lxy[200] = {0.};
    Float_t genmu_lz[200] = {0.};
    Float_t genmu_pt[200] = {0.};
    Float_t genmu_eta[200] = {0.};

    //
    // --- Output
    //
    std::string output_filename;
    TH1F* counts;
    TFile* file_out;
    TTree* tree_out;
    TTree* gen_tree_out;
};

// Constructor
my_ntuplizer::my_ntuplizer(const edm::ParameterSet& iConfig) {
    usesResource("TFileService");

    parameters = iConfig;

    // Analyzer parameters
    isCosmics = parameters.getParameter<bool>("isCosmics");
    isAOD = parameters.getParameter<bool>("isAOD");

    counts = new TH1F("counts", "", 1, 0, 1);

    dmuToken = consumes<edm::View<reco::Muon>>(
        parameters.getParameter<edm::InputTag>("displacedMuonCollection"));
    if (!isCosmics) {
        prunedGenToken = consumes<edm::View<reco::GenParticle>>(
            parameters.getParameter<edm::InputTag>("prunedGenParticles"));
    }

    triggerBits_ = consumes<edm::TriggerResults>(parameters.getParameter<edm::InputTag>("bits"));
}

// Destructor
my_ntuplizer::~my_ntuplizer() {}

// beginJob (Before first event)
void my_ntuplizer::beginJob() {
    std::cout << "Begin Job" << std::endl;

    // Init the file and the TTree
    output_filename = parameters.getParameter<std::string>("nameOfOutput");
    file_out = new TFile(output_filename.c_str(), "RECREATE");
    tree_out = new TTree("Events", "Events");
    gen_tree_out = new TTree("GenParticles", "GenParticles");

    // Load HLT paths
    HLTPaths_.push_back("HLT_L2Mu10_NoVertex_NoBPTX3BX");
    HLTPaths_.push_back("HLT_L2Mu10_NoVertex_NoBPTX");

    // TTree branches
    tree_out->Branch("event", &event, "event/I");
    tree_out->Branch("lumiBlock", &lumiBlock, "lumiBlock/I");
    tree_out->Branch("run", &run, "run/I");

    // ----------------------------------
    // displacedMuons
    // ----------------------------------
    tree_out->Branch("ndmu", &ndmu, "ndmu/I");
    tree_out->Branch("dmu_isDSA", dmu_isDSA, "dmu_isDSA[ndmu]/I");
    tree_out->Branch("dmu_isDGL", dmu_isDGL, "dmu_isDGL[ndmu]/I");
    tree_out->Branch("dmu_isDTK", dmu_isDTK, "dmu_isDTK[ndmu]/I");
    tree_out->Branch("dmu_isMatchesValid", dmu_isMatchesValid, "dmu_isMatchesValid[ndmu]/I");
    tree_out->Branch("dmu_numberOfMatches", dmu_numberOfMatches, "dmu_numberOfMatches[ndmu]/I");
    tree_out->Branch("dmu_numberOfChambers", dmu_numberOfChambers, "dmu_numberOfChambers[ndmu]/I");
    tree_out->Branch("dmu_numberOfChambersCSCorDT", dmu_numberOfChambersCSCorDT,
                     "dmu_numberOfChambersCSCorDT[ndmu]/I");
    tree_out->Branch("dmu_numberOfMatchedStations", dmu_numberOfMatchedStations,
                     "dmu_numberOfMatchedStations[ndmu]/I");
    tree_out->Branch("dmu_numberOfMatchedRPCLayers", dmu_numberOfMatchedRPCLayers,
                     "dmu_numberOfMatchedRPCLayers[ndmu]/I");
    // dmu_dsa
    tree_out->Branch("dmu_dsa_pt", dmu_dsa_pt, "dmu_dsa_pt[ndmu]/F");
    tree_out->Branch("dmu_dsa_eta", dmu_dsa_eta, "dmu_dsa_eta[ndmu]/F");
    tree_out->Branch("dmu_dsa_phi", dmu_dsa_phi, "dmu_dsa_phi[ndmu]/F");
    tree_out->Branch("dmu_dsa_ptError", dmu_dsa_ptError, "dmu_dsa_ptError[ndmu]/F");
    tree_out->Branch("dmu_dsa_dxy", dmu_dsa_dxy, "dmu_dsa_dxy[ndmu]/F");
    tree_out->Branch("dmu_dsa_dz", dmu_dsa_dz, "dmu_dsa_dz[ndmu]/F");
    tree_out->Branch("dmu_dsa_normalizedChi2", dmu_dsa_normalizedChi2,
                     "dmu_dsa_normalizedChi2[ndmu]/F");
    tree_out->Branch("dmu_dsa_charge", dmu_dsa_charge, "dmu_dsa_charge[ndmu]/F");
    tree_out->Branch("dmu_dsa_nMuonHits", dmu_dsa_nMuonHits, "dmu_dsa_nMuonHits[ndmu]/I");
    tree_out->Branch("dmu_dsa_nValidMuonHits", dmu_dsa_nValidMuonHits,
                     "dmu_dsa_nValidMuonHits[ndmu]/I");
    tree_out->Branch("dmu_dsa_nValidMuonDTHits", dmu_dsa_nValidMuonDTHits,
                     "dmu_dsa_nValidMuonDTHits[ndmu]/I");
    tree_out->Branch("dmu_dsa_nValidMuonCSCHits", dmu_dsa_nValidMuonCSCHits,
                     "dmu_dsa_nValidMuonCSCHits[ndmu]/I");
    tree_out->Branch("dmu_dsa_nValidMuonRPCHits", dmu_dsa_nValidMuonRPCHits,
                     "dmu_dsa_nValidMuonRPCHits[ndmu]/I");
    tree_out->Branch("dmu_dsa_nValidStripHits", dmu_dsa_nValidStripHits,
                     "dmu_dsa_nValidStripHits[ndmu]/I");
    tree_out->Branch("dmu_dsa_nhits", dmu_dsa_nhits, "dmu_dsa_nhits[ndmu]/I");
    tree_out->Branch("dmu_dsa_dtStationsWithValidHits", dmu_dsa_dtStationsWithValidHits,
                     "dmu_dsa_dtStationsWithValidHits[ndmu]/I");
    tree_out->Branch("dmu_dsa_cscStationsWithValidHits", dmu_dsa_cscStationsWithValidHits,
                     "dmu_dsa_cscStationsWithValidHits[ndmu]/I");
    tree_out->Branch("dmu_dsa_nsegments", dmu_dsa_nsegments, "dmu_dsa_nsegments[ndmu]/I");
    tree_out->Branch("dmu_dsa_passTagID", dmu_dsa_passTagID, "dmu_dsa_passTagID[ndmu]/O");
    tree_out->Branch("dmu_dsa_hasProbe", dmu_dsa_hasProbe, "dmu_dsa_hasProbe[ndmu]/O");
    tree_out->Branch("dmu_dsa_probeID", dmu_dsa_probeID, "dmu_dsa_probeID[ndmu]/I");
    tree_out->Branch("dmu_dsa_cosAlpha", dmu_dsa_cosAlpha, "dmu_dsa_cosAlpha[ndmu]/F");
    // dmu_dgl
    tree_out->Branch("dmu_dgl_pt", dmu_dgl_pt, "dmu_dgl_pt[ndmu]/F");
    tree_out->Branch("dmu_dgl_eta", dmu_dgl_eta, "dmu_dgl_eta[ndmu]/F");
    tree_out->Branch("dmu_dgl_phi", dmu_dgl_phi, "dmu_dgl_phi[ndmu]/F");
    tree_out->Branch("dmu_dgl_ptError", dmu_dgl_ptError, "dmu_dgl_ptError[ndmu]/F");
    tree_out->Branch("dmu_dgl_dxy", dmu_dgl_dxy, "dmu_dgl_dxy[ndmu]/F");
    tree_out->Branch("dmu_dgl_dz", dmu_dgl_dz, "dmu_dgl_dz[ndmu]/F");
    tree_out->Branch("dmu_dgl_normalizedChi2", dmu_dgl_normalizedChi2,
                     "dmu_dgl_normalizedChi2[ndmu]/F");
    tree_out->Branch("dmu_dgl_charge", dmu_dgl_charge, "dmu_dgl_charge[ndmu]/F");
    tree_out->Branch("dmu_dgl_nMuonHits", dmu_dgl_nMuonHits, "dmu_dgl_nMuonHits[ndmu]/I");
    tree_out->Branch("dmu_dgl_nValidMuonHits", dmu_dgl_nValidMuonHits,
                     "dmu_dgl_nValidMuonHits[ndmu]/I");
    tree_out->Branch("dmu_dgl_nValidMuonDTHits", dmu_dgl_nValidMuonDTHits,
                     "dmu_dgl_nValidMuonDTHits[ndmu]/I");
    tree_out->Branch("dmu_dgl_nValidMuonCSCHits", dmu_dgl_nValidMuonCSCHits,
                     "dmu_dgl_nValidMuonCSCHits[ndmu]/I");
    tree_out->Branch("dmu_dgl_nValidMuonRPCHits", dmu_dgl_nValidMuonRPCHits,
                     "dmu_dgl_nValidMuonRPCHits[ndmu]/I");
    tree_out->Branch("dmu_dgl_nValidStripHits", dmu_dgl_nValidStripHits,
                     "dmu_dgl_nValidStripHits[ndmu]/I");
    tree_out->Branch("dmu_dgl_nhits", dmu_dgl_nhits, "dmu_dgl_nhits[ndmu]/I");
    tree_out->Branch("dmu_dgl_passTagID", dmu_dgl_passTagID, "dmu_dgl_passTagID[ndmu]/O");
    tree_out->Branch("dmu_dgl_hasProbe", dmu_dgl_hasProbe, "dmu_dgl_hasProbe[ndmu]/O");
    tree_out->Branch("dmu_dgl_probeID", dmu_dgl_probeID, "dmu_dgl_probeID[ndmu]/I");
    tree_out->Branch("dmu_dgl_cosAlpha", dmu_dgl_cosAlpha, "dmu_dgl_cosAlpha[ndmu]/F");

    // Trigger branches
    for (unsigned int ihlt = 0; ihlt < HLTPaths_.size(); ihlt++) {
        tree_out->Branch(TString(HLTPaths_[ihlt]), &triggerPass[ihlt]);
    }
    // ----------------------------------
    // additional variables by Marco
    // ----------------------------------
    tree_out->Branch("dmu_t0_InOut", dmu_t0_InOut, "dmu_t0_InOut[ndmu]/F");
    tree_out->Branch("dmu_t0_OutIn", dmu_t0_OutIn, "dmu_t0_OutIn[ndmu]/F");
    tree_out->Branch("dmu_dsa_isProbe", dmu_dsa_isProbe, "dmu_dsa_isProbe[ndmu]/O");
    tree_out->Branch("dmu_dgl_isProbe", dmu_dgl_isProbe, "dmu_dgl_isProbe[ndmu]/O");
    // LLP gen matching
    tree_out->Branch("dmu_dsa_genMatched", dmu_dsa_genMatched, "dmu_dsa_genMatched[ndmu]/O");
    tree_out->Branch("dmu_dgl_genMatched", dmu_dgl_genMatched, "dmu_dgl_genMatched[ndmu]/O");
    tree_out->Branch("dmu_dsa_genMatchingMultiplicity", dmu_dsa_genMatchingMultiplicity,
                     "dmu_dsa_genMatchingMultiplicity[ndmu]/I");
    tree_out->Branch("dmu_dgl_genMatchingMultiplicity", dmu_dgl_genMatchingMultiplicity,
                     "dmu_dgl_genMatchingMultiplicity[ndmu]/I");
    tree_out->Branch("dmu_dsa_genMatchingDeltaR", dmu_dsa_genMatchingDeltaR,
                     "dmu_dsa_genMatchingDeltaR[ndmu]/F");
    tree_out->Branch("dmu_dgl_genMatchingDeltaR", dmu_dgl_genMatchingDeltaR,
                     "dmu_dgl_genMatchingDeltaR[ndmu]/F");
    tree_out->Branch("dmu_dsa_genMatchedID", dmu_dsa_genMatchedID, "dmu_dsa_genMatchedID[ndmu]/I");
    tree_out->Branch("dmu_dgl_genMatchedID", dmu_dgl_genMatchedID, "dmu_dgl_genMatchedID[ndmu]/I");
    gen_tree_out->Branch("ngenmu", &ngenmu, "ngenmu/I");
    gen_tree_out->Branch("genmu_genMatched", genmu_genMatched, "genmu_genMatched[ngenmu]/O");
    gen_tree_out->Branch("genmu_lxy", genmu_lxy, "genmu_lxy[ngenmu]/F");
    gen_tree_out->Branch("genmu_lz", genmu_lz, "genmu_lz[ngenmu]/F");
    gen_tree_out->Branch("genmu_pt", genmu_pt, "genmu_pt[ngenmu]/F");
    gen_tree_out->Branch("genmu_eta", genmu_eta, "genmu_eta[ngenmu]/F");
}

// endJob (After event loop has finished)
void my_ntuplizer::endJob() {
    std::cout << "End Job" << std::endl;
    file_out->cd();
    tree_out->Write();
    gen_tree_out->Write();
    counts->Write();
    file_out->Close();
}

// fillDescriptions
void my_ntuplizer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.setUnknown();
    descriptions.addDefault(desc);
}

// Analyze (per event)
void my_ntuplizer::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
    iEvent.getByToken(dmuToken, dmuons);
    iEvent.getByToken(triggerBits_, triggerBits);

    // Count number of events read
    counts->Fill(0);

    // -> Event info
    event = iEvent.id().event();
    lumiBlock = iEvent.id().luminosityBlock();
    run = iEvent.id().run();

    // ----------------------------------
    // LLP Signal - Gen Matching
    // ----------------------------------
    if (!isCosmics) {
        iEvent.getByToken(prunedGenToken, prunedGen);
        // Loop over reco muons and try to match them to gen muons
        ndmu = 0;
        for (unsigned int i = 0; i < dmuons->size(); i++) {
            const reco::Muon& dmuon(dmuons->at(i));
            dmu_dsa_genMatched[ndmu] = false;
            dmu_dgl_genMatched[ndmu] = false;
            dmu_dsa_genMatchingMultiplicity[ndmu] = 0;
            dmu_dgl_genMatchingMultiplicity[ndmu] = 0;
            dmu_dsa_genMatchingDeltaR[ndmu] = 9999.;
            dmu_dgl_genMatchingDeltaR[ndmu] = 9999.;
            dmu_dsa_genMatchedID[ndmu] = -1;
            dmu_dgl_genMatchedID[ndmu] = -1;
            if (dmuon.isGlobalMuon()) {
                const reco::Track* globalTrack = (dmuon.combinedMuon()).get();
                // Loop over prunedGenParticles
                ngenmu = 0;
                for (unsigned int j = 0; j < prunedGen->size(); j++) {
                    const reco::GenParticle& genPart(prunedGen->at(j));
                    if (genPart.status() != 1 ||             // must be stable
                        abs(genPart.pdgId()) != 13 ||        // must be muon
                        !hasMotherWithPdgId(&genPart, 1023)  // must be from Z_d
                    ) {
                        continue;
                    }
                    float dR = reco::deltaR(*globalTrack, genPart);
                    if (dR < 0.5) {
                        dmu_dgl_genMatched[ndmu] = true;
                        dmu_dgl_genMatchingMultiplicity[ndmu]++;
                        dmu_dgl_genMatchingDeltaR[ndmu] =
                            std::min(dmu_dgl_genMatchingDeltaR[ndmu], dR);
                        dmu_dgl_genMatchedID[ndmu] = ngenmu;
                    }
                    ngenmu++;
                }
            }
            if (dmuon.isStandAloneMuon()) {
                const reco::Track* outerTrack = (dmuon.standAloneMuon()).get();
                // Loop over prunedGenParticles
                ngenmu = 0;
                for (unsigned int j = 0; j < prunedGen->size(); j++) {
                    const reco::GenParticle& genPart(prunedGen->at(j));
                    if (genPart.status() != 1 ||             // must be stable
                        abs(genPart.pdgId()) != 13 ||        // must be muon
                        !hasMotherWithPdgId(&genPart, 1023)  // must be from Z_d
                    ) {
                        continue;
                    }
                    float dR = reco::deltaR(*outerTrack, genPart);
                    if (dR < 0.5) {
                        dmu_dsa_genMatched[ndmu] = true;
                        dmu_dsa_genMatchingMultiplicity[ndmu]++;
                        dmu_dsa_genMatchingDeltaR[ndmu] =
                            std::min(dmu_dsa_genMatchingDeltaR[ndmu], dR);
                        dmu_dsa_genMatchedID[ndmu] = ngenmu;
                    }
                    ngenmu++;
                }
            }
            ndmu++;
        }  // End loop over reco muons

        // Fill gen_tree_out
        ngenmu = 0;
        for (unsigned int j = 0; j < prunedGen->size(); j++) {
            const reco::GenParticle& genPart(prunedGen->at(j));
            if (genPart.status() != 1 ||             // must be stable
                abs(genPart.pdgId()) != 13 ||        // must be muon
                !hasMotherWithPdgId(&genPart, 1023)  // must be from Z_d
            ) {
                continue;
            }
            genmu_genMatched[ngenmu] = false;
            // A gen muon is gen matched if its index is anywhere in the
            //  dmu_dsa/dgl_genMatchedID array
            for (unsigned int i = 0; i < dmuons->size(); i++) {
                Int_t dsaGenID = dmu_dsa_genMatchedID[i];
                Int_t dglGenID = dmu_dgl_genMatchedID[i];
                
                if ((dsaGenID != -1 && dsaGenID == ngenmu) || 
                    (dglGenID != -1 && dglGenID == ngenmu)) {
                    genmu_genMatched[ngenmu] = true;
                    break; // No need to continue once we find a match
                }
            }
            // Fill the genmu variables
            genmu_lxy[ngenmu] = XYZVector(genPart.vx(), genPart.vy(), genPart.vz()).rho();
            genmu_lz[ngenmu] = genPart.vz();
            genmu_pt[ngenmu] = genPart.pt();
            genmu_eta[ngenmu] = genPart.eta();

            ngenmu++;
        }
        gen_tree_out->Fill();
    }

    // ----------------------------------
    // displacedMuons Collection
    // ----------------------------------
    ndmu = 0;
    for (unsigned int i = 0; i < dmuons->size(); i++) {
        // std::cout << " - - ndmu: " << ndmu << std::endl;
        const reco::Muon& dmuon(dmuons->at(i));
        dmu_isDGL[ndmu] = dmuon.isGlobalMuon();
        dmu_isDSA[ndmu] = dmuon.isStandAloneMuon();
        dmu_isDTK[ndmu] = dmuon.isTrackerMuon();
        dmu_isMatchesValid[ndmu] = dmuon.isMatchesValid();
        dmu_numberOfMatches[ndmu] = dmuon.numberOfMatches();
        dmu_numberOfChambers[ndmu] = dmuon.numberOfChambers();
        dmu_numberOfChambersCSCorDT[ndmu] = dmuon.numberOfChambersCSCorDT();
        dmu_numberOfMatchedStations[ndmu] = dmuon.numberOfMatchedStations();
        dmu_numberOfMatchedRPCLayers[ndmu] = dmuon.numberOfMatchedRPCLayers();
        dmu_t0_InOut[ndmu] = dmuon.time().timeAtIpInOut;
        dmu_t0_OutIn[ndmu] = dmuon.time().timeAtIpOutIn;
        dmu_dsa_isProbe[ndmu] = false;
        dmu_dgl_isProbe[ndmu] = false;

        // Access the DGL track associated to the displacedMuon
        // std::cout << "isGlobalMuon: " << dmuon.isGlobalMuon() << std::endl;
        if (dmuon.isGlobalMuon()) {
            const reco::Track* globalTrack = (dmuon.combinedMuon()).get();
            dmu_dgl_pt[ndmu] = globalTrack->pt();
            dmu_dgl_eta[ndmu] = globalTrack->eta();
            dmu_dgl_phi[ndmu] = globalTrack->phi();
            dmu_dgl_ptError[ndmu] = globalTrack->ptError();
            dmu_dgl_dxy[ndmu] = globalTrack->dxy();
            dmu_dgl_dz[ndmu] = globalTrack->dz();
            dmu_dgl_normalizedChi2[ndmu] = globalTrack->normalizedChi2();
            dmu_dgl_charge[ndmu] = globalTrack->charge();
            dmu_dgl_nMuonHits[ndmu] = globalTrack->hitPattern().numberOfMuonHits();
            dmu_dgl_nValidMuonHits[ndmu] = globalTrack->hitPattern().numberOfValidMuonHits();
            dmu_dgl_nValidMuonDTHits[ndmu] = globalTrack->hitPattern().numberOfValidMuonDTHits();
            dmu_dgl_nValidMuonCSCHits[ndmu] = globalTrack->hitPattern().numberOfValidMuonCSCHits();
            dmu_dgl_nValidMuonRPCHits[ndmu] = globalTrack->hitPattern().numberOfValidMuonRPCHits();
            dmu_dgl_nValidStripHits[ndmu] = globalTrack->hitPattern().numberOfValidStripHits();
            dmu_dgl_nhits[ndmu] = globalTrack->hitPattern().numberOfValidHits();
        } else {
            dmu_dgl_pt[ndmu] = 0;
            dmu_dgl_eta[ndmu] = 0;
            dmu_dgl_phi[ndmu] = 0;
            dmu_dgl_ptError[ndmu] = 0;
            dmu_dgl_dxy[ndmu] = 0;
            dmu_dgl_dz[ndmu] = 0;
            dmu_dgl_normalizedChi2[ndmu] = 0;
            dmu_dgl_charge[ndmu] = 0;
            dmu_dgl_nMuonHits[ndmu] = 0;
            dmu_dgl_nValidMuonHits[ndmu] = 0;
            dmu_dgl_nValidMuonDTHits[ndmu] = 0;
            dmu_dgl_nValidMuonCSCHits[ndmu] = 0;
            dmu_dgl_nValidMuonRPCHits[ndmu] = 0;
            dmu_dgl_nValidStripHits[ndmu] = 0;
            dmu_dgl_nhits[ndmu] = 0;
        }

        // Access the DSA track associated to the displacedMuon
        // std::cout << "isStandAloneMuon: " << dmuon.isStandAloneMuon() << std::endl;
        if (dmuon.isStandAloneMuon()) {
            const reco::Track* outerTrack = (dmuon.standAloneMuon()).get();
            dmu_dsa_pt[ndmu] = outerTrack->pt();
            dmu_dsa_eta[ndmu] = outerTrack->eta();
            dmu_dsa_phi[ndmu] = outerTrack->phi();
            dmu_dsa_ptError[ndmu] = outerTrack->ptError();
            dmu_dsa_dxy[ndmu] = outerTrack->dxy();
            dmu_dsa_dz[ndmu] = outerTrack->dz();
            dmu_dsa_normalizedChi2[ndmu] = outerTrack->normalizedChi2();
            dmu_dsa_charge[ndmu] = outerTrack->charge();
            dmu_dsa_nMuonHits[ndmu] = outerTrack->hitPattern().numberOfMuonHits();
            dmu_dsa_nValidMuonHits[ndmu] = outerTrack->hitPattern().numberOfValidMuonHits();
            dmu_dsa_nValidMuonDTHits[ndmu] = outerTrack->hitPattern().numberOfValidMuonDTHits();
            dmu_dsa_nValidMuonCSCHits[ndmu] = outerTrack->hitPattern().numberOfValidMuonCSCHits();
            dmu_dsa_nValidMuonRPCHits[ndmu] = outerTrack->hitPattern().numberOfValidMuonRPCHits();
            dmu_dsa_nValidStripHits[ndmu] = outerTrack->hitPattern().numberOfValidStripHits();
            dmu_dsa_nhits[ndmu] = outerTrack->hitPattern().numberOfValidHits();
            dmu_dsa_dtStationsWithValidHits[ndmu] =
                outerTrack->hitPattern().dtStationsWithValidHits();
            dmu_dsa_cscStationsWithValidHits[ndmu] =
                outerTrack->hitPattern().cscStationsWithValidHits();
            if (isAOD) {
                // Number of DT+CSC segments
                unsigned int nsegments = 0;
                for (trackingRecHit_iterator hit = outerTrack->recHitsBegin();
                     hit != outerTrack->recHitsEnd(); ++hit) {
                    if (!(*hit)->isValid()) continue;
                    DetId id = (*hit)->geographicalId();
                    if (id.det() != DetId::Muon) continue;
                    if (id.subdetId() == MuonSubdetId::DT || id.subdetId() == MuonSubdetId::CSC) {
                        nsegments++;
                    }
                }
                dmu_dsa_nsegments[ndmu] = nsegments;
            }
        } else {
            dmu_dsa_pt[ndmu] = 0;
            dmu_dsa_eta[ndmu] = 0;
            dmu_dsa_phi[ndmu] = 0;
            dmu_dsa_ptError[ndmu] = 0;
            dmu_dsa_dxy[ndmu] = 0;
            dmu_dsa_dz[ndmu] = 0;
            dmu_dsa_normalizedChi2[ndmu] = 0;
            dmu_dsa_charge[ndmu] = 0;
            dmu_dsa_nMuonHits[ndmu] = 0;
            dmu_dsa_nValidMuonHits[ndmu] = 0;
            dmu_dsa_nValidMuonDTHits[ndmu] = 0;
            dmu_dsa_nValidMuonCSCHits[ndmu] = 0;
            dmu_dsa_nValidMuonRPCHits[ndmu] = 0;
            dmu_dsa_nValidStripHits[ndmu] = 0;
            dmu_dsa_nhits[ndmu] = 0;
            dmu_dsa_dtStationsWithValidHits[ndmu] = 0;
            dmu_dsa_cscStationsWithValidHits[ndmu] = 0;
            dmu_dsa_nsegments[ndmu] = 0;
        }

        ndmu++;
        // std::cout << "End muon" << std::endl;
    }

    // ----------------------------------
    // Tag and probe code - Cosmics only
    // ----------------------------------
    if (isCosmics) {
        ndmu = 0;
        for (unsigned int i = 0; i < dmuons->size(); i++) {
            const reco::Muon& dmuon(dmuons->at(i));
            // Access the DGL track associated to the displacedMuon
            // std::cout << "isGlobalMuon: " << dmuon.isGlobalMuon() << std::endl;
            if (dmuon.isGlobalMuon()) {
                const reco::Track* globalTrack = (dmuon.combinedMuon()).get();
                // Fill tag and probe variables
                //   First, reset the variables
                dmu_dgl_passTagID[ndmu] = false;
                dmu_dgl_hasProbe[ndmu] = false;
                dmu_dgl_probeID[ndmu] = 0;
                dmu_dgl_cosAlpha[ndmu] = 0.;
                // Check if muon passes tag ID
                dmu_dgl_passTagID[ndmu] = passTagID(globalTrack, "DGL");
                if (dmu_dgl_passTagID[ndmu]) {
                    // Search probe
                    XYZVector v_tag =
                        XYZVector(globalTrack->px(), globalTrack->py(), globalTrack->pz());
                    const reco::Muon* muonProbeTemp =
                        nullptr;  // pointer for temporal probe (initialized to nullptr)
                    for (unsigned int j = 0; j < dmuons->size();
                         j++) {  // Loop over the rest of the muons
                        if (i == j) { continue; }
                        const reco::Muon& muonProbeCandidate(dmuons->at(j));
                        if (!muonProbeCandidate.isGlobalMuon()) { continue; }  // Get only dgls
                        const reco::Track* trackProbeCandidate =
                            (muonProbeCandidate.combinedMuon()).get();
                        if (passProbeID(trackProbeCandidate, v_tag, "DGL")) {
                            XYZVector v_probe =
                                XYZVector(trackProbeCandidate->px(), trackProbeCandidate->py(),
                                          trackProbeCandidate->pz());
                            if (!dmu_dgl_hasProbe[ndmu]) {
                                dmu_dgl_hasProbe[ndmu] = true;
                                muonProbeTemp = &(dmuons->at(j));
                                dmu_dgl_probeID[ndmu] = j;
                                dmu_dgl_cosAlpha[ndmu] = cos(Angle(v_probe, v_tag));
                            } else {
                                const reco::Track* trackProbeTemp =
                                    (muonProbeTemp->combinedMuon()).get();
                                if (trackProbeCandidate->pt() > trackProbeTemp->pt()) {
                                    muonProbeTemp = &(dmuons->at(j));
                                    dmu_dgl_probeID[ndmu] = j;
                                    dmu_dgl_cosAlpha[ndmu] = cos(Angle(v_probe, v_tag));
                                } else {
                                    std::cout << ">> Probe candidate " << j << " has lower pt than "
                                              << dmu_dgl_probeID[ndmu] << std::endl;
                                }
                            }
                        }
                    }
                }
            } else {
                dmu_dgl_passTagID[ndmu] = false;
                dmu_dgl_hasProbe[ndmu] = false;
                dmu_dgl_probeID[ndmu] = 0;
                dmu_dgl_cosAlpha[ndmu] = 0.;
            }
            // Access the DSA track associated to the displacedMuon
            // std::cout << "isStandAloneMuon: " << dmuon.isStandAloneMuon() << std::endl;
            if (dmuon.isStandAloneMuon()) {
                const reco::Track* outerTrack = (dmuon.standAloneMuon()).get();
                // Fill tag and probe variables
                //   First, reset the variables
                dmu_dsa_passTagID[ndmu] = false;
                dmu_dsa_hasProbe[ndmu] = false;
                dmu_dsa_probeID[ndmu] = 0;
                dmu_dsa_cosAlpha[ndmu] = 0.;
                // Check if muon passes tag ID
                dmu_dsa_passTagID[ndmu] = passTagID(outerTrack, "DSA");
                if (dmu_dsa_passTagID[ndmu]) {
                    // Search probe
                    XYZVector v_tag =
                        XYZVector(outerTrack->px(), outerTrack->py(), outerTrack->pz());
                    const reco::Muon* muonProbeTemp =
                        nullptr;  // pointer for temporal probe (initialized to nullptr)
                    for (unsigned int j = 0; j < dmuons->size(); j++) {
                        // Loop over the rest of the muons
                        if (i == j) { continue; }
                        const reco::Muon& muonProbeCandidate(dmuons->at(j));
                        if (!muonProbeCandidate.isStandAloneMuon()) { continue; }  // Get only dsas
                        const reco::Track* trackProbeCandidate =
                            (muonProbeCandidate.standAloneMuon()).get();
                        if (passProbeID(trackProbeCandidate, v_tag, "DSA")) {
                            XYZVector v_probe =
                                XYZVector(trackProbeCandidate->px(), trackProbeCandidate->py(),
                                          trackProbeCandidate->pz());
                            if (!dmu_dsa_hasProbe[ndmu]) {
                                dmu_dsa_hasProbe[ndmu] = true;
                                muonProbeTemp = &(dmuons->at(j));
                                dmu_dsa_probeID[ndmu] = j;
                                dmu_dsa_cosAlpha[ndmu] = cos(Angle(v_probe, v_tag));
                            } else {
                                const reco::Track* trackProbeTemp =
                                    (muonProbeTemp->standAloneMuon()).get();
                                if (trackProbeCandidate->pt() > trackProbeTemp->pt()) {
                                    muonProbeTemp = &(dmuons->at(j));
                                    dmu_dsa_probeID[ndmu] = j;
                                    dmu_dsa_cosAlpha[ndmu] = cos(Angle(v_probe, v_tag));
                                } else {
                                    std::cout << ">> Probe candidate " << j << " has lower pt than "
                                              << dmu_dsa_probeID[ndmu] << std::endl;
                                }
                            }
                        }
                    }
                }
            } else {
                dmu_dsa_passTagID[ndmu] = false;
                dmu_dsa_hasProbe[ndmu] = false;
                dmu_dsa_probeID[ndmu] = 0;
                dmu_dsa_cosAlpha[ndmu] = 0.;
            }
            ndmu++;
        }
        // After all tag and probes are assigned definitively, we can set the isProbe variables
        ndmu = 0;
        for (unsigned int i = 0; i < dmuons->size(); i++) {
            if (dmu_isDGL[ndmu] && dmu_dgl_hasProbe[ndmu]) {
                dmu_dgl_isProbe[dmu_dgl_probeID[ndmu]] = true;
            }
            if (dmu_isDSA[ndmu] && dmu_dsa_hasProbe[ndmu]) {
                dmu_dsa_isProbe[dmu_dsa_probeID[ndmu]] = true;
            }
            ndmu++;
        }
    }

    // Check if trigger fired:
    const edm::TriggerNames& names = iEvent.triggerNames(*triggerBits);
    unsigned int ipath = 0;
    for (auto path : HLTPaths_) {
        std::string path_v = path + "_v";
        // std::cout << path << "\t" << std::endl;
        bool fired = false;
        for (unsigned int itrg = 0; itrg < triggerBits->size(); ++itrg) {
            TString TrigPath = names.triggerName(itrg);
            if (!triggerBits->accept(itrg)) continue;
            if (!TrigPath.Contains(path_v)) { continue; }
            fired = true;
        }
        triggerPass[ipath] = fired;
        ipath++;
    }

    //-> Fill tree
    tree_out->Fill();
}

DEFINE_FWK_MODULE(my_ntuplizer);
