#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cmath>
#include <time.h>
#include <algorithm>
#include "split.h"

#include <omp.h>

#include "api/BamMultiReader.h"
#include "readPileUp.h"

using namespace std;
using namespace BamTools;

struct regionDat{
  int seqidIndex ;
  int start      ;
  int end        ;
};

struct indvDat{
  bool   support ;
  string genotype      ;
  int    genotypeIndex ; 
  int    nReads        ;
  int    mappedPairs   ;
  int    nAboveAvg     ;
  int    notMapped     ;
  int    mateMissing   ;
  int    sameStrand    ;
  int    nClipping     ; 
  int    mateCrossChromosome ;
  int    maxLength      ;
  double    nBad        ;
  double    nGood       ;
  double insertSum ;
  double insertMean ;
  double lengthSum ;
  double clipped ;
  vector<double> inserts;
  vector<double> hInserts;
  vector<double> gls;
  vector< BamAlignment > alignments;
  map<string, int> badFlag;
  vector<int> MapQ;
  map<int, vector<string> > cluster;
};


struct insertDat{
  map<string, double> mus; // mean of insert length for each indvdual across 1e6 reads
  map<string, double> sds;  // standard deviation
  map<string, double> lq ;  // 25% of data
  map<string, double> up ;  // 75% of the data
} insertDists;

struct global_opts {
  vector<string> targetBams    ;
  vector<string> backgroundBams;
  vector<string> all           ;
  int            nthreads      ;
  string         seqid         ;
  string         bed           ; 
  vector<int>    region        ; 
} globalOpts;


struct info_field{
  double lrt;

  double taf;
  double baf;
  double aaf;

  double nat;
  double nbt;

  double nab;
  double nbb;

  double tgc;
  double bgc;

};

static const char *optString ="ht:b:r:x:";

// this lock prevents threads from printing on top of each other

omp_lock_t lock;

void initInfo(info_field * s){
  s->lrt = 0;
  s->taf = 0.000001;
  s->baf = 0.000001;
  s->aaf = 0.000001;
  s->nat = 0;
  s->nbt = 0;
  s->nab = 0;
  s->nbb = 0;
  s->tgc = 0;
  s->bgc = 0;
}

void initIndv(indvDat * s){
  s->support             = false;
  s->genotype            = "./.";
  s->genotypeIndex       = -1;
  s->nBad                = 0;
  s->nGood               = 0;
  s->nReads              = 0;
  s->nAboveAvg           = 0;
  s->notMapped           = 0;
  s->mappedPairs         = 0;
  s->mateMissing         = 0;
  s->insertSum           = 0;
  s->sameStrand          = 0;
  s->maxLength           = 0;
  s->mateCrossChromosome = 0;
  s->lengthSum           = 0;
  s->clipped             = 0;
  s->alignments.clear();
  s->inserts.clear();
  s->hInserts.clear();
  s->badFlag.clear();
  s->MapQ.clear();
}

void printHeader(void){
  cout << "##fileformat=VCFv4.1"                                                                                                                  << endl;
  cout << "#INFO=<LRT,Number=1,type=Float,Description=\"Likelihood Ratio Test Statistic\">"                                                       << endl;
  cout << "#INFO=<AF,Number=3,type=Float,Description=\"Allele frequency of: target,background,combined\">" << endl;
  cout << "#INFO=<NALT,Number=2,type=Int,Description=\"Number of alternative pseuod alleles for target and background \">" << endl;
  cout << "##FORMAT=<ID=GT,Number=1,Type=String,Description=\"Pseudo genotype\">"                                                                 << endl;
  cout << "##FORMAT=<GL,Number=A,type=Float,Desciption=\"Genotype likelihood \">"                                                                 << endl;
  cout << "##FORMAT=<ID=NG,Number=1,Type=Int,Description=\"Number of reads supporting no SV\">"                                                      << endl;
  cout << "##FORMAT=<ID=NB,Number=1,Type=Int,Description=\"Number of reads supporting no SV\">"                                                      << endl;
  cout << "##FORMAT=<ID=CL,Number=1,Type=Int,Description=\"Number of bases that have been softclipped\">"                                            << endl;
  cout << "##FORMAT=<ID=DP,Number=1,Type=Int,Description=\"Number of reads with mapping quality greater than 0\">"                                << endl;
  cout << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT" << "\t";

  for(int b = 0; b < globalOpts.all.size(); b++){
    cout << globalOpts.all[b] ;
    if(b < globalOpts.all.size() - 1){
      cout << "\t";
    }
  }
  cout << endl;  
}

string join(vector<string> strings){

  string joined = "";

  for(vector<string>::iterator sit = strings.begin(); sit != strings.end(); sit++){
    joined = joined + " " + (*sit) + "\n";
  }

  return joined;

}

void printVersion(void){
  cerr << "Version 0.0.1 ; Zev Kronenberg; zev.kronenberg@gmail.com " << endl;
  cerr << endl;
}

void printHelp(void){
  cerr << "usage  : WHAM-BAM -x <INT> -r <STRING>     -e <STRING>  -t <STRING>    -b <STRING>   " << endl << endl;
  cerr << "example: WHAM-BAM -x 20    -r chr1:0-10000 -e genes.bed -t a.bam,b.bam -b c.bam,d.bam" << endl << endl; 

  cerr << "required: t <STRING> -- comma separated list of target bam files"           << endl ;
  cerr << "option  : b <STRING> -- comma separated list of background bam files"       << endl ;
  cerr << "option  : r <STRING> -- a genomic region in the format \"seqid:start-end\"" << endl ;
  cerr << "option  : x <INT>    -- set the number of threads, otherwise max          " << endl ; 
  cerr << "option  : e <STRING> -- a bedfile that defines regions to score           " << endl ; 
  cerr << endl;
  printVersion();
}

void parseOpts(int argc, char** argv){
  int opt = 0;

  globalOpts.bed = "NA";

  opt = getopt(argc, argv, optString);

  while(opt != -1){
    switch(opt){
    case 'e':
      {
	globalOpts.bed = optarg;
	cerr << "INFO: WHAM-BAM will only score within bed coordiates provided: " << globalOpts.bed << endl;
	break;
      }
    case 'x':
      {
	globalOpts.nthreads = atoi(((string)optarg).c_str());
	cerr << "INFO: OpenMP will roughly use " << globalOpts.nthreads << " threads" << endl;
	break;
      }
    case 't':
      {
	globalOpts.targetBams     = split(optarg, ",");
	cerr << "INFO: target bams:\n" << join(globalOpts.targetBams) ;
	break;
      }
    case 'b':
      {
	globalOpts.backgroundBams = split(optarg, ",");
	cerr << "INFO: background bams:\n" << join(globalOpts.backgroundBams) ;
	break;
      }
    case 'h':
      {
	printHelp();
	exit(1);
      }
    case 'r':
      {
	vector<string> tmp_region = split(optarg, ":");
	vector<string> start_end = split(tmp_region[1], "-");

	globalOpts.seqid = tmp_region[0];
	globalOpts.region.push_back(atoi(start_end[0].c_str()));
	globalOpts.region.push_back(atoi(start_end[1].c_str()));
		
	cerr << "INFO: region set to: " <<   globalOpts.seqid << ":" <<   globalOpts.region[0] << "-" <<  globalOpts.region[1] << endl;
	
	if(globalOpts.region.size() != 2){
	  cerr << "FATAL: incorrectly formatted region." << endl;
	  cerr << "FATAL: wham is now exiting."          << endl;
	  exit(1);
	}
	break;
      }
    case '?':
      {
	printHelp();
	exit(1);
      }  
  default:
    {
      cerr << "FATAL: Failure to parse command line options." << endl;
      cerr << "FATAL: Now exiting wham." << endl;
      printHelp();
      exit(1);
    }
    }
    opt = getopt( argc, argv, optString );
  }
  if( globalOpts.targetBams.empty() && globalOpts.backgroundBams.empty() ){
    cerr << "FATAL: Failure to specify target and/or background bam files." << endl;
    cerr << "FATAL: Now exiting wham." << endl;
    printHelp();
    exit(1);
  }
}

double bound(double v){
  if(v <= 0.00001){
    return  0.00001;
  }
  if(v >= 0.99999){
    return 0.99999;
  }
  return v;
}


double mean(vector<double> & data){

  double sum = 0;
  double n   = 0;

  for(vector<double>::iterator it = data.begin(); it != data.end(); it++){
    sum += (*it);
    n += 1;
  }
  return sum / n;
}

double var(vector<double> & data, double mu){
  double variance = 0;

  for(vector<double>::iterator it = data.begin(); it != data.end(); it++){
    variance += pow((*it) - mu,2);
  }

  return variance / (data.size() - 1);
}

bool grabInsertLengths(string file){

  BamReader bamR;
  BamAlignment al;

  vector<double> alIns;

  double clipped  = 0;
  double naligned = 0;

  bamR.Open(file);

  int i = 1;
  while(i < 100000 && bamR.GetNextAlignment(al) && abs(double(al.InsertSize)) < 10000){
    if(al.IsFirstMate() && al.IsMapped() && al.IsMateMapped()){
      i += 1;
      alIns.push_back(abs(double(al.InsertSize)));
    }
    if(al.IsMapped()){
      naligned += 1;
      vector< CigarOp > cd = al.CigarData;

      if(cd.back().Type == 'S' || cd.back().Type == 'H' ){
        clipped += double(cd.back().Length) / double (al.Length);
      }
      if(cd.front().Type == 'S' || cd.front().Type == 'H'){
	clipped += double(cd.front().Length) / double (al.Length);
      }

    }
  }

  bamR.Close();

  double mu       = mean(alIns        );
  double variance = var(alIns, mu     );
  double sd       = sqrt(variance     );

  insertDists.mus[file] = mu;
  insertDists.sds[file] = sd;

  cerr << "INFO: mean insert length, number of reads, file : "
       << insertDists.mus[file] << ", "
       << insertDists.sds[file] << ", "
       << i  << ", "
       << file << endl;
  cerr << "INFO: fraction of total length clipped, file : " << clipped / naligned
       << ", "
       << file << endl;

  return true;

}


bool getInsertDists(void){

  bool flag = true;

  for(int i = 0; i < globalOpts.all.size(); i++){
    flag = grabInsertLengths(globalOpts.all[i]);
  }
  
  return true;
  
}

void prepBams(BamMultiReader & bamMreader, string group){

  int errorFlag    = 3;
  string errorMessage ;

  vector<string> files;

  if(group == "target"){
    files = globalOpts.targetBams;
  }
  if(group == "background"){
    files = globalOpts.backgroundBams;
  }
  if(group == "all"){
        files = globalOpts.all;
  }
  if(files.empty()){
    cerr << "FATAL: no files ?" << endl;
    exit(1);
  }

  bool attempt = true;
  int  tried   = 0   ;
  
  while(attempt && tried < 500){
    
    //    sleep(int(rand() % 10)+1);

    if( bamMreader.Open(files) && bamMreader.LocateIndexes() ){
      attempt = false;
    }
    else{
      tried += 1;
    }
  }
  if(attempt == true){
    cerr << "FATAL: unable to open BAMs or indices after: " << tried << " attempts"  << endl;  
    cerr << "Bamtools error message:\n" <<  bamMreader.GetErrorString()  << endl;
    cerr << "INFO : try using less CPUs in the -x option" << endl;
    exit(1);
  }

}

double logLbinomial(double x, double n, double p){

  double ans = lgamma(n+1)-lgamma(x+1)-lgamma(n-x+1) + x * log(p) + (n-x) * log(1-p);
  return ans;

}


double logDbeta(double alpha, double beta, double x){
  
  double ans = 0;

  ans += lgamma(alpha + beta) - ( lgamma(alpha) + lgamma(beta) );
  ans += log( pow(x, (alpha -1) ) ) + log( pow( (1-x) , (beta-1)));
  
  cerr << "alpha: " << alpha << "\t" << "beta: " << beta << "\t" << "x: " << x << "\t" << ans;
  cerr << endl;

  return ans;

}

double totalLL(vector<double> & data, double alpha, double beta){

  double total = 0;

  for(vector<double>::iterator it = data.begin(); it != data.end(); it++){
    total += logDbeta(alpha, beta, (*it));
  }
  return total;
}

double methodOfMoments(double mu, double var, double * aHat, double * bHat){

  double mui = 1 - mu;
  double right = ( (mu*mui / var) - 1 );

  (*aHat)  = mu *  right;
  (*bHat)  = mui * right; 
  
}

double unphred(double p){
  return pow(10, (-p/10));
}


bool processGenotype(indvDat * idat, double * totalAlt){

  string genotype = "./.";

  double aal = 0;
  double abl = 0;
  double bbl = 0;

  if(idat->nReads < 3){
    idat->gls.push_back(-255.0);
    idat->gls.push_back(-255.0);
    idat->gls.push_back(-255.0);
    return true;
  }

  double nref = 0.0;
  double nalt = 0.0;

  int ri = 0;

  for(map< string, int >::iterator rit = idat->badFlag.begin(); rit != idat->badFlag.end(); rit++){

    double mappingP = unphred(idat->MapQ[ri]);

    if( idat->badFlag[rit->first] == 1 && idat->support == true){
      nalt += 1;
      aal += log((2-2) * (1-mappingP) + (2*mappingP)) ;
      abl += log((2-1) * (1-mappingP) + (1*mappingP)) ;
      bbl += log((2-0) * (1-mappingP) + (0*mappingP)) ;
    }
    else{
      nref += 1;
      aal += log((2 - 2)*mappingP + (2*(1-mappingP)));
      abl += log((2 - 1)*mappingP + (1*(1-mappingP)));
      bbl += log((2 - 0)*mappingP + (0*(1-mappingP)));
    }
    ri++;
  }

  idat->nBad  = nalt;
  idat->nGood = nref;

  aal = aal - log(pow(2,idat->nReads));
  abl = abl - log(pow(2,idat->nReads));
  bbl = bbl - log(pow(2,idat->nReads));

  if(nref == 0){
    aal = -255.0;
    abl = -255.0;
  }
  if(nalt == 0){
    abl = -255.0;
    bbl = -255.0;
  }

  double max = aal;
  genotype     = "0/0";
  idat->genotypeIndex = 0;

  if(abl > max){
    genotype = "0/1";
    idat->genotypeIndex = 1;
    (*totalAlt) += 1;
    max = abl;
  }
  if(bbl > max){
    genotype = "1/1";
    idat->genotypeIndex = 2;
    (*totalAlt) += 2;
  }

  idat->genotype = genotype;
  idat->gls.push_back(aal);
  idat->gls.push_back(abl);
  idat->gls.push_back(bbl);

  return true;

  //   cerr << (*geno) << "\t" << aal << "\t" << abl << "\t" << bbl << "\t" << nref << "\t"  << endl;
}

bool loadIndv(map<string, indvDat*> & ti, 
	      readPileUp & pileup, 
	      global_opts localOpts, 
	      insertDat & localDists, 
	      long int * pos,
	      map <long int, int> & clusters
	      ){    

  
  for(list<BamAlignment>::iterator r = pileup.currentData.begin(); r != pileup.currentData.end(); r++){
   
    if((*r).Position > *pos){
      continue;
    }
    
    string fname = (*r).Filename;
    
    //  cerr << pos << ": " 
    //  << (*r).Name << "\t" 
    //  << (*r).Position 
    //  << "\t" 
    // << (*r).GetEndPosition() ;

    int bad = 0;
    
    ti[fname]->alignments.push_back(*r);
    
    ti[fname]->nReads += 1;
  
    vector< CigarOp > cd = (*r).CigarData;
    
    ti[fname]->lengthSum += (*r).Length;
    
    if(cd.back().Type == 'S' || cd.back().Type == 'H'){
      ti[fname]->clipped += cd.back().Length;
      int location = (*r).GetEndPosition();
      ti[fname]->cluster[location].push_back((*r).Name);
    }
    
    if(cd.front().Type == 'S' || cd.front().Type == 'H'){
      ti[fname]->clipped += cd.front().Length;
      int location = (*r).GetEndPosition();
      ti[fname]->cluster[location].push_back((*r).Name);
    }
    
    if(!(*r).IsMateMapped()){
      ti[fname]->mateMissing  += (!(*r).IsMateMapped());
      bad = 1;
    }
    
    if((*r).IsMapped() && (*r).IsMateMapped()){

      ti[fname]->insertSum    += abs(double((*r).InsertSize));
      ti[fname]->mappedPairs  += 1;
      
      if(( (*r).IsReverseStrand() && (*r).IsMateReverseStrand() ) || ( !(*r).IsReverseStrand() && !(*r).IsMateReverseStrand() )){
	bad = 1;
	ti[fname]->sameStrand += 1;
      }
      
      double ilength = abs ( double ( (*r).InsertSize ));
      
      double iDiff = abs ( ilength - localDists.mus[(*r).Filename] );
      
      ti[fname]->inserts.push_back(ilength);
      
      if(iDiff > (3.0 * insertDists.sds[(*r).Filename]) ){
	bad = 1;
	ti[fname]->nAboveAvg += 1;
	ti[fname]->hInserts.push_back(ilength);
      }
    }
  
    //    cerr << endl;

    ti[fname]->badFlag[(*r).Name] = bad;
    ti[fname]->MapQ.push_back((*r).MapQuality);
   
  }

  // looping over indviduals
  
  for(map < string, indvDat*>::iterator indvs = ti.begin(); indvs != ti.end(); indvs++){
    // looping over clusters
    for( map< int, vector<string> >::iterator ci = ti[indvs->first]->cluster.begin(); ci != ti[indvs->first]->cluster.end(); ci++){
      if(ci->second.size() > 1){
	// setting support
        ti[indvs->first]->support = true;
	clusters[((long) ci->first) + 1] += ci->second.size();
	// looping over reads
	for(vector<string>::iterator readName = ti[indvs->first]->cluster[ci->first].begin(); readName != ti[indvs->first]->cluster[ci->first].end(); readName++ ){
	  ti[indvs->first]->badFlag[(*readName)] = 1;
	}
      }
    }
  }

  //  cerr << "loading indv" << endl;
  return true;
}


bool cleanUp( map < string, indvDat*> & ti, global_opts localOpts){
  for(vector<string>::iterator all = localOpts.all.begin(); all != localOpts.all.end(); all++ ){
    delete ti[*all];
  }
}

bool loadInfoField(map<string, indvDat*> dat, info_field * info, global_opts & opts){
  
  for(int b = 0; b < opts.backgroundBams.size(); b++){

    if( dat[opts.backgroundBams[b]]->genotypeIndex == -1){
      continue;
    }
    info->nat += 2 - dat[opts.backgroundBams[b]]->genotypeIndex;
    info->nbt +=     dat[opts.backgroundBams[b]]->genotypeIndex;
    info->tgc += 1;
  }

  for(int t = 0; t < opts.targetBams.size(); t++){
    if(dat[opts.targetBams[t]]->genotypeIndex == -1){
      continue;
    }
    info->nab += 2 - dat[opts.targetBams[t]]->genotypeIndex;
    info->nbb +=     dat[opts.targetBams[t]]->genotypeIndex;
    info->bgc += 1;
  }

  info->taf += info->nbt / (info->nat + info->nbt);
  info->baf += info->nbb / (info->nab + info->nbb);
  info->aaf += (info->nbt + info->nbb) / (info->nat + info->nbt + info->nab + info->nbb);

  double alt  = logLbinomial(info->nbt, (info->tgc * 2), info->taf) + logLbinomial(info->nbb, (2* info->bgc), info->baf);
  double null = logLbinomial(info->nbt, (info->tgc * 2), info->aaf) + logLbinomial(info->nbb, (2* info->bgc), info->aaf);

  info->lrt = 2 * (alt - null);
  
  return true;

}


string infoText(info_field * info){
  
  stringstream ss;

  ss << "LRT=" << info->lrt << ";";
  ss << "AF="  << info->taf << "," << info->baf << "," << info->aaf << ";";
  ss << "GC="  << info->tgc << "," << info->bgc << ";";

  return ss.str();

}

bool score(string seqid, 
	   long int * pos, 
	   readPileUp & totalDat, 
	   insertDat & localDists, 
	   string & results, 
	   global_opts localOpts){
  
  // cerr << "scoring" << endl;
  // cerr << (*pos) << endl;
  
  map < string, indvDat*> ti;
  
  for(int t = 0; t < localOpts.all.size(); t++){
    indvDat * i;
    i = new indvDat;
    initIndv(i);
    ti[localOpts.all[t]] = i;
  }
  
  map<long int, int> clusters;

  loadIndv(ti, totalDat, localOpts, localDists, pos, clusters);

  if(clusters.find( ( *pos)+1 ) == clusters.end()){
    cleanUp(ti, localOpts);
    return true;
  }

  stringstream cl;

  for(map<long int, int>::iterator zit = clusters.begin(); zit != clusters.end(); zit++){
    cl << zit->first << "->" << zit->second << "," ;
  }

  double nAlt = 0;

  for(int t = 0; t < localOpts.all.size(); t++){
    processGenotype(ti[localOpts.all[t]], &nAlt);
  }

  if(nAlt == 0 ){
    cleanUp(ti, localOpts);
    return true;
  }

  info_field * info = new info_field; 

  initInfo(info);

  loadInfoField(ti, info, localOpts);

  string infoToPrint = infoText(info);
  
  stringstream tmpOutput;

  tmpOutput  << seqid       << "\t" ;       // CHROM
  tmpOutput  << (*pos) +1   << "\t" ;       // POS
  tmpOutput  << "."         << "\t" ;       // ID
  tmpOutput  << "NA"        << "\t" ;       // REF
  tmpOutput  << "SV"        << "\t" ;       // ALT
  tmpOutput  << "."         << "\t" ;       // QUAL
  tmpOutput  << "."         << "\t" ;       // FILTER
  tmpOutput  << infoToPrint << ""  ;
  tmpOutput  << cl.str()    << "\t";
  
  tmpOutput  << "GT:GL:NB:NG:CL:DP" << "\t" ;
      
  for(int t = 0; t < localOpts.all.size(); t++){
    tmpOutput << ti[localOpts.all[t]]->genotype 
	      << ":" << ti[localOpts.all[t]]->gls[0]
	      << "," << ti[localOpts.all[t]]->gls[1]
	      << "," << ti[localOpts.all[t]]->gls[2]
	      << ":" << ti[localOpts.all[t]]->nBad
	      << ":" << ti[localOpts.all[t]]->nGood
              << ":" << ti[localOpts.all[t]]->clipped
	      << ":" << ti[localOpts.all[t]]->nReads
	      << "\t";
    if(t < localOpts.all.size() - 1){
      tmpOutput << "\t";
    }
  }
  
  tmpOutput << endl;
  
  results.append(tmpOutput.str());
 
  cleanUp(ti, localOpts);
  

  delete info;
  
  return true;
}

 
bool runRegion(int seqidIndex, int start, int end, vector< RefData > seqNames){
  
  string regionResults;

  omp_set_lock(&lock);

  global_opts localOpts = globalOpts;
  insertDat localDists = insertDists;

  omp_unset_lock(&lock);

  BamMultiReader All;
  
  prepBams(All, "all");

  if(!All.SetRegion(seqidIndex, start, seqidIndex, end)){
    return false;
  }

  BamAlignment al     ;
  readPileUp allPileUp;

  if(! All.GetNextAlignment(al)){
    All.Close();
    return false;
  }

  long int currentPos = -1;

  allPileUp.processAlignment(al , currentPos);

  bool getNextAl     = true;

  while(1){  
    while(currentPos >= allPileUp.CurrentStart  && getNextAl){
      getNextAl = All.GetNextAlignment(al);
      if(al.IsMapped() &&  al.MapQuality > 0 ){
	allPileUp.processAlignment(al, currentPos);
      }
    }
    if(getNextAl == false){
      break;
    }
    
    allPileUp.purgePast();

    if(allPileUp.softClipAtEnds()){
      if(! score(seqNames[seqidIndex].RefName, 
		 &currentPos, 
		 allPileUp,
		 localDists, 
		 regionResults, 
		 localOpts )){
	cerr << "FATAL: problem during scoring" << endl;
	cerr << "FATAL: wham exiting"           << endl;
	exit(1);
      }
    }
    
    currentPos += 1;
    
    if(regionResults.size() > 100000){
      omp_set_lock(&lock);
      cout << regionResults;
      omp_unset_lock(&lock);
      regionResults.clear(); 
    }
  }

  omp_set_lock(&lock);

  cout << regionResults;

  omp_unset_lock(&lock);
  
  regionResults.clear();
  
  All.Close();

  return true;
}

bool loadBed(vector<regionDat*> & features, RefVector seqs){

  map<string, int> seqidToInt;

  int index = 0;

  for(vector< RefData >::iterator sit = seqs.begin(); sit != seqs.end(); sit++){

    seqidToInt[ (*sit).RefName ] = index;

    index+=1;
  }

  ifstream featureFile (globalOpts.bed);

  string line;

  if(featureFile.is_open()){

    while(getline(featureFile, line)){

      vector<string> region = split(line, "\t");

      int start = atoi(region[1].c_str()) ;
      int end   = atoi(region[2].c_str()) ;
  
      regionDat * r = new regionDat;
      r->seqidIndex = seqidToInt[region[0]];
      r->start      = start;
      r->end        = end  ;
      features.push_back(r);
    }
  }
  else{
    return false;
  }
  return true;
}

int main(int argc, char** argv) {

  omp_init_lock(&lock);

  srand((unsigned)time(NULL));

  globalOpts.nthreads = -1;

  parseOpts(argc, argv);
  
  if(globalOpts.nthreads == -1){
  }
  else{
    omp_set_num_threads(globalOpts.nthreads);
  }
 
  globalOpts.all.reserve(globalOpts.targetBams.size()  + globalOpts.backgroundBams.size());
  globalOpts.all.insert( globalOpts.all.end(), globalOpts.targetBams.begin(), globalOpts.targetBams.end() );
  globalOpts.all.insert( globalOpts.all.end(), globalOpts.backgroundBams.begin(), globalOpts.backgroundBams.end() );

  BamMultiReader allReader;
  

  // checking for bam 
  prepBams(allReader, "all");
  SamHeader SH = allReader.GetHeader();
  if(!SH.HasSortOrder()){
    cerr << "FATAL: sorted bams must have the @HD SO: tag in each SAM header." << endl;
    exit(1);
  }
  


  allReader.Close();

  if(!getInsertDists()){
    cerr << "FATAL: " << "problem while generating insert lengths dists" << endl;
    exit(1);
  }

  cerr << "INFO: generated distributions" << endl;
  prepBams(allReader, "all");  
  RefVector sequences = allReader.GetReferenceData();
  allReader.Close();

  printHeader();

  int seqidIndex = 0;

  if(globalOpts.region.size() == 2){
    for(vector< RefData >::iterator sit = sequences.begin(); sit != sequences.end(); sit++){      
      if((*sit).RefName == globalOpts.seqid){
	break;
      }
      seqidIndex += 1;
    }
  }

  if(seqidIndex != 0){
    if(! runRegion(seqidIndex, globalOpts.region[0], globalOpts.region[1], sequences)){
      cerr << "WARNING: region failed to run properly." << endl;
    }
    cerr << "INFO: WHAM-BAM finished normally." << endl;
    return 0;
  }
  
  vector< regionDat* > regions; 
  if(globalOpts.bed == "NA"){
    for(vector< RefData >::iterator sit = sequences.begin(); sit != sequences.end(); sit++){
      int start = 500;
      if((*sit).RefLength < 2000){
	cerr << "WARNING: " << (*sit).RefName << " is too short for WHAM-BAM: " << (*sit).RefLength << endl;
	continue;
      }
      
      for(;start < ( (*sit).RefLength - 500) ; start += 10000000){
	regionDat * chunk = new regionDat;
	chunk->seqidIndex = seqidIndex;
	chunk->start      = start;
	chunk->end        = start + 10000000 ;
	regions.push_back(chunk);
      }
      regionDat * lastChunk = new regionDat;
      lastChunk->seqidIndex = seqidIndex;
      lastChunk->start = start;
      lastChunk->end   = (*sit).RefLength;
      seqidIndex += 1;
      if(start < (*sit).RefLength){
	regions.push_back(lastChunk);
      }
    }
  }
  else{
    loadBed(regions, sequences);
  }

 #pragma omp parallel for
  
  for(int re = 0; re < regions.size(); re++){

    omp_set_lock(&lock);
    cerr << "INFO: running region: " << sequences[regions[re]->seqidIndex].RefName << ":" << regions[re]->start << "-" << regions[re]->end << endl;
    omp_unset_lock(&lock);

    if(! runRegion( regions[re]->seqidIndex, regions[re]->start, regions[re]->end, sequences)){
      omp_set_lock(&lock);
      cerr << "WARNING: region failed to run properly: " 
	   << sequences[regions[re]->seqidIndex].RefName 
	   << ":"  << regions[re]->start << "-" 
	   << regions[re]->end 
	   <<  endl;
      omp_unset_lock(&lock);
    }

  }

    //    (*chunk) = NULL;
    //    delete (*chunk);


  cerr << "INFO: WHAM-BAM finished normally." << endl;
  return 0;
}
