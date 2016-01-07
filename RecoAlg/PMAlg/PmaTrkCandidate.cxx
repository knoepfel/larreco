/**
 *  @file   PmaTrkCandidate.cxx
 *
 *  @author D.Stefan and R.Sulej
 * 
 *  @brief  Track finding helper for the Projection Matching Algorithm
 *
 *          Candidate for 3D track. Used to test 2D cluster associations, validadion result, MSE value.
 *          See PmaTrack3D.h file for details.
 */

#include "RecoAlg/PMAlg/PmaTrkCandidate.h"

#include "messagefacility/MessageLogger/MessageLogger.h"

pma::TrkCandidate::TrkCandidate(void) :
	fParent(-1), fTrack(0), fKey(-1), fTreeId(-1),
	fMse(0), fValidation(0),
	fGood(false)
{
}
// ------------------------------------------------------

pma::TrkCandidate::TrkCandidate(pma::Track3D* trk, int key, int tid) :
	fParent(-1), fTrack(trk), fKey(key), fTreeId(tid),
	fMse(0), fValidation(0),
	fGood(false)
{
}
// ------------------------------------------------------

void pma::TrkCandidate::SetTrack(pma::Track3D* trk)
{
	if (fTrack) delete fTrack;
	fTrack = trk;
}
// ------------------------------------------------------

void pma::TrkCandidate::DeleteTrack(void)
{
	if (fTrack) delete fTrack;
	fTrack = 0;
}
// ------------------------------------------------------
// ------------------------------------------------------
// ------------------------------------------------------

int pma::getCandidateIndex(pma::trk_candidates const & tracks, pma::Track3D const * candidate)
{
	for (size_t t = 0; t < tracks.size(); ++t)
		if (tracks[t].Track() == candidate) return t;
	return -1;
}

void pma::setParentDaughterConnections(pma::trk_candidates& tracks)
{
	for (size_t t = 0; t < tracks.size(); ++t)
	{
		pma::Track3D const * trk = tracks[t].Track();
		pma::Node3D const * firstNode = trk->Nodes().front();
		if (firstNode->Prev())
		{
			pma::Track3D const * parentTrk = static_cast< pma::Segment3D* >(firstNode->Prev())->Parent();
			tracks[t].SetParent(pma::getCandidateIndex(tracks, parentTrk));
		}
		for (auto node : trk->Nodes())
			for (size_t i = 0; i < node->NextCount(); ++i)
		{
			pma::Track3D const * daughterTrk = static_cast< pma::Segment3D* >(node->Next(i))->Parent();
			if (daughterTrk != trk)
			{
				int idx = pma::getCandidateIndex(tracks, daughterTrk);
				if (idx >= 0) tracks[t].Daughters().push_back((size_t)idx);
			}
		}
	}
}
// ------------------------------------------------------

void pma::setTreeId(pma::trk_candidates & tracks, int id, size_t trkIdx, bool isRoot)
{
	pma::Track3D* trk = tracks[trkIdx].Track();
	pma::Node3D* vtx = trk->Nodes().front();
	pma::Segment3D* segThis = 0;
	pma::Segment3D* seg = 0;

	if (!isRoot)
	{
		segThis = trk->NextSegment(vtx);
		if (segThis) vtx = static_cast< pma::Node3D* >(segThis->Next());
	}

	while (vtx)
	{
		segThis = trk->NextSegment(vtx);

		for (size_t i = 0; i < vtx->NextCount(); i++)
		{
			seg = static_cast< pma::Segment3D* >(vtx->Next(i));
			if (seg != segThis)
			{
				int idx = pma::getCandidateIndex(tracks, seg->Parent());

				if (idx >= 0) pma::setTreeId(tracks, id, idx, false);
				else mf::LogError("pma::setTreeIds") << "Branch of the tree not found in tracks collection.";
			}
		}

		if (segThis) vtx = static_cast< pma::Node3D* >(segThis->Next());
		else break;
	}

	tracks[trkIdx].SetTreeId(id);
}

int pma::setTreeIds(pma::trk_candidates & tracks)
{
	for (auto & t : tracks) t.SetTreeId(-1);

	int id = 0;
	for (auto & t : tracks)
	{
		if (t.TreeId() >= 0) continue;

		int rootTrkIdx = pma::getCandidateIndex(tracks, t.Track()->GetRoot());

		if (rootTrkIdx >= 0) pma::setTreeId(tracks, id, rootTrkIdx);
		else mf::LogError("pma::setTreeIds") << "Root of the tree not found in tracks collection.";

		id++;
	}

	return id;
}
// ------------------------------------------------------

void pma::flipTreesToCoordinate(pma::trk_candidates & tracks, size_t coordinate)
{
	std::map< int, std::vector< pma::Track3D* > > toFlip;
	std::map< int, double > minVal;

	pma::setTreeIds(tracks);
	for (auto & t : tracks)
	{
		int tid = t.TreeId();
		if (minVal.find(tid) == minVal.end()) minVal[tid] = 1.0e12;

		TVector3 pFront(t.Track()->front()->Point3D()); pFront.SetY(-pFront.Y());
		TVector3 pBack(t.Track()->back()->Point3D()); pBack.SetY(-pBack.Y());

		bool pushed = false;
		if (pFront[coordinate] < minVal[tid]) { minVal[tid] = pFront[coordinate]; toFlip[tid].push_back(t.Track()); pushed = true; }
		if (pBack[coordinate] < minVal[tid]) { minVal[tid] = pBack[coordinate]; if (!pushed) toFlip[tid].push_back(t.Track()); }
	}

	for (auto & tEntry : toFlip)
		if (tEntry.first >= 0)
	{
		size_t attempts = 0;
		while (!tEntry.second.empty())
		{
			pma::Track3D* trk = tEntry.second.back();
			tEntry.second.pop_back();

			TVector3 pFront(trk->front()->Point3D()); pFront.SetY(-pFront.Y());
			TVector3 pBack(trk->back()->Point3D()); pBack.SetY(-pBack.Y());

			if (pFront[coordinate] > pBack[coordinate])
			{
				if (trk->CanFlip()) { trk->Flip(); break; } // go to the next tree if managed to flip
			}
			else break; // good orientation, go to the next tree

			if (attempts++ > 2) break; // do not try all the tracks in the queue...
		}
	}
}
// ------------------------------------------------------

pma::Track3D* pma::getTreeCopy(pma::trk_candidates & dst, const pma::trk_candidates & src, size_t trkIdx, bool isRoot)
{
	pma::Track3D* trk = src[trkIdx].Track();
	pma::Node3D* vtx = trk->Nodes().front();
	pma::Segment3D* segThis = 0;
	pma::Segment3D* seg = 0;

	int key = src[trkIdx].Key();
	int tid = src[trkIdx].TreeId();

	pma::Track3D* trkCopy = new pma::Track3D(*trk);
	pma::Node3D* vtxCopy = trkCopy->Nodes().front();
	pma::Segment3D* segThisCopy = 0;
	trkCopy->SetPrecedingTrack(0);
	trkCopy->SetSubsequentTrack(0);

	dst.emplace_back(pma::TrkCandidate(trkCopy, key, tid));

	if (!isRoot)
	{
		segThis = trk->NextSegment(vtx);
		if (segThis) vtx = static_cast< pma::Node3D* >(segThis->Next());

		segThisCopy = trkCopy->NextSegment(vtxCopy);
		if (segThisCopy) vtxCopy = static_cast< pma::Node3D* >(segThisCopy->Next());
	}

	while (vtx)
	{
		segThis = trk->NextSegment(vtx);
		segThisCopy = trkCopy->NextSegment(vtxCopy);

		for (size_t i = 0; i < vtx->NextCount(); i++)
		{
			seg = static_cast< pma::Segment3D* >(vtx->Next(i));
			if (seg != segThis)
			{
				int idx = pma::getCandidateIndex(src, seg->Parent());

				if (idx >= 0)
				{
					pma::Track3D* branchCopy = pma::getTreeCopy(dst, src, idx, false);
					if (!branchCopy->AttachTo(vtxCopy, true)) // no flip
					mf::LogError("pma::getTreeCopy") << "Branch copy cannot be attached to the tree.";
				}
				else mf::LogError("pma::getTreeCopy") << "Branch of the tree not found in source collection.";
			}
		}

		if (segThis) vtx = static_cast< pma::Node3D* >(segThis->Next());
		else break;

		if (segThisCopy) vtxCopy = static_cast< pma::Node3D* >(segThisCopy->Next());
	}

	return trkCopy;
}

