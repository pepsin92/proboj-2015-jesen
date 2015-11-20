
#include <ostream>
#include <vector>
#include <queue>
#include <set>
#include <algorithm>
#include <cmath>
using namespace std;

#include "common.h"
#include "update.h"
#include "util.h"

static ostream* g_observation;
void zapniObservation(ostream* observation) { g_observation = observation; }

bool lezivMape (const Mapa& mapa, const Bod& b) {
  return (b.x>=0 && b.x<=mapa.w && b.y>=0 && b.y<=mapa.h);
}

bool zrazka (const FyzikalnyObjekt& A, const FyzikalnyObjekt& B) {
  Bod spojnica = A.pozicia - B.pozicia;
  return A.polomer + B.polomer > spojnica.dist();
}

Bod odpal1 (const FyzikalnyObjekt& A, const FyzikalnyObjekt& B) {
  // zrazka 1. typu, co oddeluje mergnute objekty
  //
  // vracia zrychlenie A v dosledku kolizie

  if (!zrazka(A,B)) {
    return Bod();
  }
  if (A.koliznyLevel > B.koliznyLevel) {
    return Bod();
  }
  double cast= (A.koliznyLevel==B.koliznyLevel ? 1.0 : 2.0);
  
  double dotykVzdial = A.polomer+B.polomer;
  Bod spojnica = A.pozicia - B.pozicia;
  Bod ciel = spojnica * (dotykVzdial / spojnica.dist());

  Bod projekcia = (A.pozicia+A.rychlost)-(B.pozicia+B.rychlost);
  projekcia = projekcia*spojnica;

  Bod vysl = (ciel-projekcia)*cast;
  if (vysl/spojnica < 0) {
    return Bod();
  }
  return vysl;
}

Bod odpal2 (const FyzikalnyObjekt& A, const FyzikalnyObjekt& B) {
  // zrazka 2. typu, co NEoddeluje mergnute objekty
  //
  // vracia zrychlenie A v dosledku kolizie

  if (!zrazka(A,B)) {
    return Bod();
  }
  if (A.koliznyLevel > B.koliznyLevel) {
    return Bod();
  }
  double cast= (A.koliznyLevel==B.koliznyLevel ? 1.0 : 2.0);

  Bod spojnica= A.pozicia-B.pozicia;
  Bod projekcia= (A.pozicia+A.rychlost)-(B.pozicia+B.rychlost);
  projekcia= projekcia*spojnica;

  Bod vysl= (spojnica-projekcia)*cast;
  if (vysl/spojnica < 0) {
    return Bod();
  }
  return vysl;
}
  


void odsimuluj(const Mapa& mapa, Stav& stav, const vector<Prikaz>& akcie) {

  // odsimuluj zrazky, a pridel objektom zrychlenie
  //
  vector<FyzikalnyObjekt*> objekty;
  vector<Bod> zrychl;
  for (int t=0; t<STAV_TYPOV-1; t++) {
    for (FyzikalnyObjekt& obj : stav.obj[t]) {
      objekty.push_back(&obj);
      zrychl.push_back(Bod());
    }
  }
  for (FyzikalnyObjekt& obj : stav.obj[BOSS]) {
    // so far, bossovia su indestructible
    // aby sa mohli hybat hlupo (burat do hviezd)
    //
    objekty.push_back(&obj);
    Bod kam(obj.pozicia);
    double best= INF;
    for (int i=0; i<(int)stav.hraci.size(); i++) {
      Bod kde= stav.hraci[i].obj.pozicia;
      double vzdial= (kde-obj.pozicia).dist();
      if (vzdial < best) {
        best = vzdial;
        kam=kde;
      }
    }
    zrychl.push_back(kam);
  }
  for (Vec& vec : stav.veci) {
    objekty.push_back(&vec.obj);
    zrychl.push_back(Bod());
  }
  for (int i=0; i<(int)stav.hraci.size(); i++) {
    if (!stav.hraci[i].zije()) {
      continue;
    }
    objekty.push_back(&stav.hraci[i].obj);
    zrychl.push_back(akcie[i].acc);
  }

  for (int i=0; i<(int)objekty.size()-1; i++) {
    FyzikalnyObjekt* prvy=objekty[i];
    for (int j=i+1; j<(int)objekty.size(); j++) {
      FyzikalnyObjekt* druhy=objekty[j];
      
      Bod acc = odpal1(*prvy, *druhy);
      Bod antiacc = odpal1(*druhy, *prvy);
      double silaZrazky = acc.dist()+antiacc.dist();
      
      if (! prvy->neznicitelny() ) {
        prvy->zivoty -= druhy->sila*silaZrazky;
      }
      if (! druhy->neznicitelny() ) {
        druhy->zivoty -= prvy->sila*silaZrazky;
      }
      zrychl[i]= zrychl[i]+acc;
      zrychl[j]= zrychl[j]+antiacc;
    }
  }
  for (int i=0; i<(int)objekty.size(); i++) {
    objekty[i]->rychlost = objekty[i]->rychlost + zrychl[i];
  }

  // udrz objekty v hracom poli
  //
  int dx[4]={0,1,0,-1};
  int dy[4]={1,0,-1,0};
  for (FyzikalnyObjekt* ptr : objekty) {
    for (int smer=0; smer<4; smer++) {
      double x= ptr->pozicia.x;
      double y= ptr->pozicia.y;
      
      switch (dx[smer]) {
        case -1: x= -SENTINEL_POLOMER;
        case 1: x=mapa.w+SENTINEL_POLOMER;
      }
      switch (dy[smer]) {
        case -1: y= -SENTINEL_POLOMER;
        case 1: y=mapa.h+SENTINEL_POLOMER;
      }
      FyzikalnyObjekt sentinel(-1,Bod(x,y),Bod(),SENTINEL_POLOMER,INF,SENTINEL_SILA,INF);
      Bod acc= odpal2(*ptr,sentinel);
      ptr->rychlost= ptr->rychlost+acc;
    }
  }

  stav.cas++;
}




bool pociatocnyStav(Mapa& mapa, Stav& stav, int pocKlientov) {
  stav.cas = 0;

  if ((int)mapa.spawny.size() < pocKlientov) {
    log("prilis malo spawnov v mape");
    return false;
  }
  random_shuffle(mapa.spawny.begin(), mapa.spawny.end());
  for (int i=0; i<pocKlientov; i++) {
    Hrac novy(mapa.spawny[i]);
    stav.hraci.push_back( novy );
  }
  mapa.spawny.clear();

  for (int i=0; i<(int)mapa.objekty.size(); i++) {
    int typ = mapa.objekty[i].typ;
    if (typ > MAPA_TYPOV || typ < 0) {
      log("neznamy objekt vo vesmire");
      return false;
    }
    stav.obj[typ].push_back( mapa.objekty[i] );
  }
  mapa.objekty.clear();

  for (int i=0; i<(int)mapa.veci.size(); i++) {
    stav.veci.push_back( mapa.veci[i] );
  }
  mapa.veci.clear();

  return true;
}
