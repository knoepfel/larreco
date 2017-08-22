#include "larreco/RecoAlg/TCAlg/TCCR.h"
#include "larsim/MCCheater/BackTracker.h"

namespace tca {

  ////////////////////////////////////////////////
  void SaveCRInfo(TjStuff& tjs, MatchStruct& ms, bool prt, bool fIsRealData){

    //Check the origin of pfp
    if (tjs.SaveCRTree){
      if (fIsRealData){
        tjs.crt.cr_origin.push_back(-1);
      }
      else{
        tjs.crt.cr_origin.push_back(GetOrigin(tjs, ms));
      }
    }

    // save the xmin and xmax of each pfp
    tjs.crt.cr_pfpxmin.push_back(std::min(ms.XYZ[0][0], ms.XYZ[1][0]));
    tjs.crt.cr_pfpxmax.push_back(std::max(ms.XYZ[0][0], ms.XYZ[1][0]));

    //find max 
    const geo::TPCGeo &tpc = tjs.geom->TPC(0);
    float mindis0 = FLT_MAX;
    float mindis1 = FLT_MAX;
    if (std::abs(ms.XYZ[0][1] - tpc.MinY())<mindis0) mindis0 = std::abs(ms.XYZ[0][1] - tpc.MinY());
    if (std::abs(ms.XYZ[0][1] - tpc.MaxY())<mindis0) mindis0 = std::abs(ms.XYZ[0][1] - tpc.MaxY());
    if (std::abs(ms.XYZ[0][2] - tpc.MinZ())<mindis0) mindis0 = std::abs(ms.XYZ[0][2] - tpc.MinZ());
    if (std::abs(ms.XYZ[0][2] - tpc.MaxZ())<mindis0) mindis0 = std::abs(ms.XYZ[0][2] - tpc.MaxZ());
    if (std::abs(ms.XYZ[1][1] - tpc.MinY())<mindis1) mindis1 = std::abs(ms.XYZ[1][1] - tpc.MinY());
    if (std::abs(ms.XYZ[1][1] - tpc.MaxY())<mindis1) mindis1 = std::abs(ms.XYZ[1][1] - tpc.MaxY());
    if (std::abs(ms.XYZ[1][2] - tpc.MinZ())<mindis1) mindis1 = std::abs(ms.XYZ[1][2] - tpc.MinZ());
    if (std::abs(ms.XYZ[1][2] - tpc.MaxZ())<mindis1) mindis1 = std::abs(ms.XYZ[1][2] - tpc.MaxZ());
    //std::cout<<ms.XYZ[0][1]<<" "<<ms.XYZ[0][2]<<" "<<ms.XYZ[1][1]<<" "<<ms.XYZ[1][2]<<" "<<tpc.MinY()<<" "<<tpc.MaxY()<<" "<<tpc.MinZ()<<" "<<tpc.MaxZ()<<" "<<mindis0<<" "<<mindis1<<" "<<mindis0+mindis1<<std::endl;
    tjs.crt.cr_pfpyzmindis.push_back(mindis0+mindis1);

    if (tjs.crt.cr_pfpxmin.back()<-2||
        tjs.crt.cr_pfpxmax.back()>260||
        tjs.crt.cr_pfpyzmindis.back()<30){
      ms.CosmicScore = 1.;
    }
    else ms.CosmicScore = 0;

    /*
    float minx = FLT_MAX;
    float maxx = FLT_MIN;

    for(auto& tjID : ms.TjIDs) {
      Trajectory& tj = tjs.allTraj[tjID - 1];
      TrajPoint& beginPoint = tj.Pts[tj.EndPt[0]];
      TrajPoint& endPoint = tj.Pts[tj.EndPt[1]];
      if (beginPoint.Pos[1]/tjs.UnitsPerTick<minx){
        minx = beginPoint.Pos[1]/tjs.UnitsPerTick;
      }
      if (beginPoint.Pos[1]/tjs.UnitsPerTick>maxx){
        maxx = beginPoint.Pos[1]/tjs.UnitsPerTick;
      }
      if (endPoint.Pos[1]/tjs.UnitsPerTick<minx){
        minx = endPoint.Pos[1]/tjs.UnitsPerTick;
      }
      if (endPoint.Pos[1]/tjs.UnitsPerTick>maxx){
        maxx = endPoint.Pos[1]/tjs.UnitsPerTick;
      }
    } // tjID

    tjs.crt.cr_pfpmintick.push_back(minx);
    tjs.crt.cr_pfpmaxtick.push_back(maxx);
    */
//    std::cout<<ms.MCPartListIndex<<std::endl;
//    std::cout<<ms.XYZ[0][0]<<" "<<ms.XYZ[1][0]<<std::endl;

  }

  ////////////////////////////////////////////////
  int GetOrigin(TjStuff& tjs, MatchStruct& ms){

    art::ServiceHandle<cheat::BackTracker> bt;    

    std::map<int, float> omap; //<origin, energy>

    for(auto& tjID : ms.TjIDs) {
      
      Trajectory& tj = tjs.allTraj[tjID - 1];
      for(auto& tp : tj.Pts) {
        for(unsigned short ii = 0; ii < tp.Hits.size(); ++ii) {
          if(!tp.UseHit[ii]) continue;
          unsigned int iht = tp.Hits[ii];
          TCHit& hit = tjs.fHits[iht];
          raw::ChannelID_t channel = tjs.geom->PlaneWireToChannel((int)hit.WireID.Plane, (int)hit.WireID.Wire, (int)hit.WireID.TPC, (int)hit.WireID.Cryostat);
          double startTick = hit.PeakTime - hit.RMS;
          double endTick = hit.PeakTime + hit.RMS;
          // get a list of track IDEs that are close to this hit
          std::vector<sim::TrackIDE> tides;
          bt->ChannelToTrackIDEs(tides, channel, startTick, endTick);          
          for(auto itide = tides.begin(); itide != tides.end(); ++itide) {
            omap[bt->TrackIDToMCTruth(itide->trackID)->Origin()] += itide->energy;
          }
        }
      }
    }
    
    float maxe = -1;
    int origin = 0;
    for (auto & i : omap){
      if (i.second > maxe){
        maxe = i.second;
        origin = i.first;
      }
    }
    return origin;
  }

  ////////////////////////////////////////////////
  void ClearCRInfo(TjStuff& tjs){
    tjs.crt.cr_origin.clear();
    tjs.crt.cr_pfpxmin.clear();
    tjs.crt.cr_pfpxmax.clear();
    tjs.crt.cr_pfpyzmindis.clear();
  }
}