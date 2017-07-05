#include <fstream>
#include <iterator>
#include <algorithm>
#include <vector>
#include <iostream>
#include <string>
#include <cstring>
#include <omp.h>

using namespace std;

void xor_reconstruct(int blocksize,int ndisks,vector<int> layout,vector<bool> missing,char** data)
{
  int imissing;
  int missingnr;
  //find missing disk
  for(int i=0;i<ndisks;i++)
  {
    if(missing[i]){
      imissing=i;
      missingnr=layout[i];
      break;
    }
  }
  //cout << "reconstructing missing block of disk " << imissing << " which would have been block " << missingnr << endl;
  //find datablock or xor parity block, that is not missing
  int ilast=-1;
  for(int j=0;j<ndisks-2;j++)
  {
    for(int i=ilast+1;i<ndisks;i++)
    {
      if(!missing[i] && (layout[i]>0 || layout[i]==-1) ){
        //cout << "block in xor from " << i << " layout=" << layout[i] <<  endl;
        ilast=i;
        break;
      }
    }
    if(j==0){    
      memcpy(data[imissing],data[ilast],blocksize);
    }else{
      //cout << "xoring " << imissing << " and " << ilast << endl;
      uint64_t* result64 = (uint64_t*)data[imissing];
      uint64_t* mask64 = (uint64_t*)data[ilast];

      for(int i = blocksize/(sizeof(uint64_t)) ; i--;i > 0)
      {
        result64[i] = result64[i] ^ mask64[i];
      }
    }
  }

}

int main(int argc, char** argv)
{
  int ndisks=argc-2;
  cout << "number of disks=" << ndisks << endl;

  //hardcoded blocksize
  const int blocksize=65536;
  
  //hardcoded raid layout for raid6 on 3ware 9650 with 7 disks
  //layout[disk][blockrow]
  vector<vector <int> > layout;
  int nblocksperrow;
  if(ndisks==7){
    // positive numbers mean blocks, -1 = Xor-parity data, -2=reed-solomon, 0 ignore
    for(int i=0;i<ndisks;i++)
    {
      vector<int> tmp(ndisks);
      layout.push_back(tmp);
    }
    nblocksperrow=ndisks-2;
    int i=0;
    layout[0][i]=-1;
    layout[1][i]=-2;
    layout[2][i]=1;
    layout[3][i]=2;
    layout[4][i]=3;
    layout[5][i]=4;
    layout[6][i]=5;
    i=i+1;
    layout[0][i]=1;
    layout[1][i]=-1;
    layout[2][i]=-2;
    layout[3][i]=2;
    layout[4][i]=3;
    layout[5][i]=4;
    layout[6][i]=5;
    i=i+1;
    layout[0][i]=1;
    layout[1][i]=2;
    layout[2][i]=-1;
    layout[3][i]=-2;
    layout[4][i]=3;
    layout[5][i]=4;
    layout[6][i]=5;
    i=i+1;
    layout[0][i]=1;
    layout[1][i]=2;
    layout[2][i]=3;
    layout[3][i]=-1;
    layout[4][i]=0;
    layout[5][i]=4;
    layout[6][i]=5;
    i=i+1;
    layout[0][i]=1;
    layout[1][i]=2;
    layout[2][i]=3;
    layout[3][i]=4;
    layout[4][i]=-1;
    layout[5][i]=-2;
    layout[6][i]=5;
    i=i+1;
    layout[0][i]=1;
    layout[1][i]=2;
    layout[2][i]=3;
    layout[3][i]=4;
    layout[4][i]=5;
    layout[5][i]=-1;
    layout[6][i]=-2;
    i=i+1;
    layout[0][i]=-2;
    layout[1][i]=1;
    layout[2][i]=2;
    layout[3][i]=3;
    layout[4][i]=4;
    layout[5][i]=5;
    layout[6][i]=-1;
  }
  //get disks
  vector<string> fnames;
  vector<bool> missing;
  int nmissing=0;
  for(int i=0;i<ndisks;i++)
  {    
    string str(argv[i+1]);
    if(str.compare("missing")==0){
      nmissing++;
      missing.push_back(true);
      fnames.push_back(str);
    }else{
      missing.push_back(false);
      fnames.push_back(str);
    }
    cout << "disk " << i << "=" << str << " missing=" << missing[i]<< endl;
  }
  cout << "number of missing disks=" << nmissing << endl;
  
  vector<ifstream> files(ndisks);
  for(int i=0;i<ndisks;i++)
  {
    if(!missing[i]){
      files[i].open(fnames[i].data(),ios::in | ios::binary);
      if(!files[i].is_open()){
        cout << "could not open " << fnames[i] << endl;
        return 1;
      }
    }
  }

  //open destination
  string destname(argv[ndisks+1]);
  cout << "output=" << destname << endl;
  ofstream dest(destname,ios::out | ios::binary);


  char** data = new char*[ndisks];
  for(int i = 0; i < ndisks; ++i)
  {
    data[i] = new char[blocksize];
  }

  double ts=omp_get_wtime();
  //benchmark read ops

  if(true){
    int nb=10000;
    //individial disks
    for(int i=0;i<ndisks;i++)
    {
      ts=omp_get_wtime();
      if(!missing[i]){
        for(int j=0;j<nb;j++)
        {
          files[i].read (data[i], blocksize);
          //cout << files[i].tellg() << endl;
        }
        cout << "disk " << i << ": " << ((double)nb*blocksize/1024.0/1024.0)/(omp_get_wtime()-ts) << " MB/s." << endl; 
      }
    }
    
    //close and reopen
    for(int i=0;i<ndisks;i++)
    {
      if(!missing[i]){
        files[i].close();
        files[i].open(fnames[i].data(),ios::in | ios::binary);
        if(!files[i].is_open()){
          cout << "could not open " << fnames[i] << endl;
          return 1;
        }
      }
    }
  }


  // build data
  bool cont=true;
  unsigned long int blockscount=0,bs=0;
  unsigned long int blocksin=0;
  int blockrow=0;

  //allocate read caches
  int nbcache=10000; // 0.6GB


  char*** cache = new char**[ndisks];
  for(int i=0;i<ndisks;i++)
  {
    cache[i] = new char*[nbcache];
    for(int j=0;j<nbcache;j++)
    {
      cache[i][j] = new char[blocksize];
    }
  }
 
  ts=omp_get_wtime();
 
  while(cont)
  {
    //read in block
    //cout << "block=" << blockscount << endl;
    //cout << "blockrow=" << blockrow << endl;
    //cout << "blocksin=" << blocksin << endl;
    for(int i=0;i<ndisks;i++)
    {
      if(!missing[i]){
        if(blocksin%nbcache==0){
          for(int j=0;j<nbcache;j++)
          {
            files[i].read (cache[i][j], blocksize);
          }
        }
        memcpy(data[i],cache[i][blocksin%nbcache],blocksize);
        //files[i].read (data[i], blocksize);
        //cout << files[i].tellg() << endl;
      }
    }
    blocksin++;
    //assemble block according to layout

    // find out if we have to reassemble a missing block from parity data
    int nm=0;
    for(int i=0;i<ndisks;i++)
    {
      if(missing[i] && layout[i][blockrow]>0){
        nm++;
      }
    }

    //cout << "nm=" << nm << endl;
    if(nm==0){
      //nothing missing

    }else if (nm==1){
      //reconstruct missing block with xor
      vector<int> lr;
      for(int i=0;i<ndisks;i++)
      {
        lr.push_back(layout[i][blockrow]);
      }
      xor_reconstruct(blocksize,ndisks,lr,missing,data);      
    }else if (nm==2){
      cout << "reconstruction from reed-solomon not implemented"<< endl;
      return 1;
    }else{
      cout << "WTF!" << endl;
      return 1;
    }

    //write data
    for(int ib=1;ib<=nblocksperrow;ib++){
      for(int i=0;i<ndisks;i++)
      {
        if(layout[i][blockrow]==ib){
          dest.write(data[i],blocksize);
          blockscount++;
          break;
        }
      }
    }
    
    blockrow++;
    if(blockrow>=ndisks){
      blockrow=0;
    }
    if(blockscount%10000==0){

      double tn=omp_get_wtime();
      cout << 1.0/((tn-ts)/(blockscount-bs))*65536.0/1024.0/1024.0 << " MB/s, \t  wrote " << blockscount << " blocks " <<  (double)blockscount*65536.0/1024.0/1024.0 << " MB" << endl;
      //ts=tn;
      //bs=blockscount;
      bs=0;
    }
  }
  return 0;	
}

