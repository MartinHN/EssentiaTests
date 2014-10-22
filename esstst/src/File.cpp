//
//  File.cpp
//  esstst
//
//  Created by martin hermant on 23/07/14.
//  Copyright (c) 2014 martin hermant. All rights reserved.
//

#include "File.h"
#include "essentia.h"
#include "algorithmfactory.h"
#include "streaming/algorithms/poolstorage.h"
#include "scheduler/network.h"
#include "streaming/algorithms/vectoroutput.h"
#include "streaming/algorithms/vectorinput.h"



#include <thread>
#include <time.h>
#include "boost/filesystem.hpp"
using namespace boost::filesystem;

using namespace std;
using namespace essentia;
using namespace streaming;


#define MAXTHREADS 6


map<string,string> findAudio(string dir);
void compute(string fn,string path,string outdir,bool * i);
void aggregate(vector<Real> v,float fR,string name,Pool* p);

bool done[MAXTHREADS];


// params
int sR = 44100;

int frameSize = 4096;
int hopSize = 512;



int main(int argc, char* argv[]){
    
    long int tStart = time(NULL);
    string dir = "/Users/mhermant/Music/iTunes/iTunes Media/Music/Santana/the best instrumentals";
//    string dir = "/Users/mhermant/Documents/Work/Datasets/beatles/audio/wav/";
//    string dir = "/Users/mhermant/Documents/Work/Datasets/IOWA/theremin.music.uiowa.edu/sound files/MIS/Piano_Other/piano";
//    string dir = "/Users/mhermant/Music/iTunes/iTunes Media/Music/Robert Le Magnifique/Oh Yeah Baby.._/";
    string outdir ="/Users/mhermant/Documents/Work/Dev/openFrameworks/MTG/ViZa/bin/data/tests/";
//    string outdir ="/Users/mhermant/Documents/Work/Datasets/beatles/viza/";
    
    

    
    
    essentia::init();

    // iterate over
    map<string,string> files = findAudio(dir);
    bool init = true;
    int ended =-1;
    vector<std::thread> threads;
    int offset =0;
    int numDone =  files.size()-offset;
    map<string,string>::iterator initit =files.begin();
    for(int i = 0 ; i < offset ; i++){
        ++initit;
    }
    
    for(map<string,string>::iterator f =initit;numDone>MAXTHREADS;++f)
    {
        if(init){
        for(int i = 0; i < MAXTHREADS ; i++){
            done[i] = false;
            threads.push_back(std::thread(compute,f->first,f->second,outdir,&done[i]));
//            threads[i].detach();
            ++f;
            ;
        }
            init = false;
        }
        
        else if (ended!=-1){
            
            cout << "adding : "+ f->first + "num :"+ std::to_string(ended)<< endl;
            done[ended] = false;
            threads.erase(threads.begin()+ended);
            
            threads.insert(threads.begin()+ended,std::thread(compute,f->first,f->second,outdir,&done[ended]));
//            threads[ended].detach();
            ended = -1;
            
            
 
        }
        while(ended == -1){
            for (int  i = 0 ; i < MAXTHREADS ;++i){
                if(done[i]){
                    ended = i;
                    if(threads[ended].joinable()){
                     
                     threads[ended].join();
//                        sleep(1);
                    }
                    
                    numDone--;
                    cout << std::to_string(numDone) + " left to do" << endl;
                    break;
                }
                
            }
        }


    }
    
    for(vector<thread>::iterator it = threads.begin() ; it !=threads.end() ; ++it){
        if(it->joinable()){
           
            it->join();}
    }

    
    
    essentia::shutdown();
    printf("Time taken: %.2fs\n", (float)(time(NULL) - tStart));
}


void compute(string fn,string path, string outdir,bool * i){
   
    if(path=="" ||  fn=="")return;

    //    essentia::setDebugLevel( essentia::DebuggingModule::EAll);
    
    
    //Init
    

    
    Pool pool;
    
    
    Algorithm* sfe = AlgorithmFactory::create("SuperFluxExtractor","threshold",1,"combine",50);
    
    Algorithm * bark = AlgorithmFactory::create("BarkExtractor","frameSize",frameSize,"hopSize",hopSize,"sampleRate", sR);

    
    Algorithm * FC = AlgorithmFactory::create("FrameCutter","frameSize",frameSize,"hopSize",hopSize,"silentFrames","noise");

    
Algorithm * windowing = AlgorithmFactory::create("Windowing","type","blackmanharris92","size",frameSize);
    
    Algorithm * spectrum = AlgorithmFactory::create("Spectrum");
    Algorithm * Log = AlgorithmFactory::create("UnaryOperator","type","lin2db");
;
    
    Algorithm * enveloppe = AlgorithmFactory::create("Envelope","sampleRate", sR);

    
    Algorithm * SpectralPeaks = AlgorithmFactory::create("SpectralPeaks","sampleRate",sR,"magnitudeThreshold",-1e9);
    
    
    
    
    
    
    Algorithm * pitchSalience = AlgorithmFactory::create("PitchSalience");
    
    Algorithm * HPCP = AlgorithmFactory::create("HPCP","harmonics",0,"size",12);
    
    
    Algorithm * HPCPPeaks = AlgorithmFactory::create("PeakDetection","maxPeaks",2,"range",11,"maxPosition",11,"interpolate",false,"orderBy","amplitude","threshold",-1e9);
    

        Algorithm* audio = AlgorithmFactory::create("MonoLoader",
                                         "filename", path,
                                         "sampleRate", sR);
    
    
    pool.set("audiopath" , path);

        //link network
    
    // fft
    audio->output("audio")>>FC->input("signal");
    FC->output("frame") >> windowing->input("frame");
    windowing->output("frame") >>spectrum->input("frame");
    spectrum->output("spectrum") >>Log->input("array");
    
    //enveloppe
    audio->output("audio")>>enveloppe->input("signal");
    enveloppe->output("signal") >> PC(pool,"onsets.envelope.value");
    pool.set("onsets.envelope.frameRate",1.0/sR);
    
    //Bark
    
    audio->output("audio") >> bark->input("signal");
    vector<string> on =bark->outputNames();
    for(vector<string>::iterator n =on.begin() ; n != on.end();++n){
        if(*n=="barkbands"){
             bark->output( *n)	>> DEVNULL;
        }
            else{
        bark->output( *n)	>> DEVNULL;
        bark->output( *n)	>> PC(pool,"onsets."+ *n+".value");
        pool.set("onsets."+ *n+".frameRate",hopSize*1.0/sR);}
    }

    
    //Pitchsalience
    
    Log->output("array") >> pitchSalience->input("spectrum");
    pitchSalience->output("pitchSalience") >> PC(pool,"onsets.pitchSalience.value");
    pool.set("onsets.pitchSalience.frameRate",hopSize*1.0/sR);
    
    
    
    // HPCP
    
    spectrum->output("spectrum") >>SpectralPeaks->input("spectrum");
    SpectralPeaks->output("frequencies")>>HPCP->input("frequencies");
    SpectralPeaks->output("magnitudes")>>HPCP->input("magnitudes");
//    HPCP->output("hpcp") >> PC(pool,"onsets.HPCP.value");
//    pool.set("onsets.HPCP.frameRate",hopSize*1.0/sR);
    
    
    // F0
    HPCP->output("hpcp") >> HPCPPeaks->input("array");
    HPCPPeaks->output("positions") >> PC(pool,"onsets.mainHPCP.value");
    HPCPPeaks->output("amplitudes") >> DEVNULL;
    pool.set("onsets.mainHPCP.frameRate",hopSize*1.0/sR);
    
    
    
    // Onsets
        sfe->output("onsets")  >> PC(pool,"onsets.slice.time");
        audio->output("audio") >> sfe->input("signal");
    
    
        //run
        
        //onsets
        essentia::scheduler::Network net= essentia::scheduler::Network(audio,true);
        net.run();
    
    
    
    // array izer ...
    
    map<string,vector< vector <Real> > > arrays = pool.getVectorRealPool();
    for(map<string,vector< vector <Real> > >::iterator it = arrays.begin() ; it!=arrays.end() ; ++it){
        string na = it->first.substr(0,it->first.find_last_of("."));
                for( vector<vector <Real >>::iterator itt = it->second.begin() ; itt!=it->second.end() ; ++itt){
                    int i = 0;
                    
                    float fR = pool.getSingleRealPool().at(na+".frameRate");
                    for(vector <Real >::iterator ittt = itt->begin() ; ittt!=itt->end() ; ++ittt){
                        pool.add(na+std::to_string(i)+".value", *ittt);
                        pool.set(na+std::to_string(i)+".frameRate", fR);
                        i++;
                    }
                }
        pool.remove(it->first);
        pool.remove(na+".frameRate");
    }
    
    
    
    map<string,vector<Real>> names = pool.getRealPool();
    vector<Real> onsets = pool.getSingleVectorRealPool().at("onsets.slice.time");
    bool padWithLast = true;
    
    for(map<string,vector<Real>>::iterator n = names.begin();n!= names.end() ; ++n){

        if(n->first.find(".time") == std::string::npos ){


            string na = n->first.substr(0,n->first.find_last_of("."));
            float fR = pool.getSingleRealPool().at(na+".frameRate");
//            if(na!="onsets.envelope"){
            pool.remove(n->first);
            pool.remove(na+".frameRate");
//            }
            
            if(padWithLast){

                onsets.push_back(n->second.size()*fR);
                
                padWithLast = false;
            }
            vector<Real>::iterator beg = n->second.begin();
            for(int i = 1; i < onsets.size();i++){
                int min = (int)(onsets[i-1]*1.0 / fR);
                int max = std::min((int)n->second.size()-1,(int)(onsets[i]*1.0/fR));
                if(max<min){
                    int aaaa=0;
                }
                vector<Real> chop(beg+ min ,beg+max );
                aggregate(chop,fR,na,&pool);
            }
            

        }
    }
    
    
    pool.set("onsets.slice.time",onsets);

    //out
    essentia::standard::Algorithm * yamlOut = essentia::standard::AlgorithmFactory::create("YamlOutput","filename",outdir+fn+".json","format","json");
    yamlOut->input("pool").set(pool);
    yamlOut->compute();
    
    pool.clear();
    delete yamlOut;

    net.clear();
    
    
//    net.clear();
//    audio->reset();
    
    
    
    cout<<fn+" : ended : found"+ std::to_string(onsets.size()-1) <<endl;
    *i = true;

}


map<string,string> findAudio(string pathin){
    map<string,string> res;
    
    path dir_path = path(pathin.c_str());
        if ( !exists( dir_path ) ) return res;
        directory_iterator end_itr; // default construction yields past-the-end
    for ( boost::filesystem::directory_iterator itr( dir_path );
             itr != end_itr;
             ++itr )
        {
            if ( !is_directory(itr->status()) )
            {	string ext = itr->path().extension().string();
                string pp = itr->path().stem().string();
                string ppp =itr->path().string();
                if((ext== ".wav"|| ext == ".mp3" || ext == ".aiff") && pp!="" && ppp!="") { res[pp] = ppp;}
            }
        }
        return res;
    
}

void aggregate(vector<Real> v,float fR,string name,Pool* p){

    
    
    Real aggr;
    Real aggr2;
    essentia::standard::Algorithm * centroid =essentia::standard::AlgorithmFactory::create("Centroid");
    centroid->input("array").set(v);
    centroid->output("centroid").set(aggr);
    centroid->compute();
    p->add(name+".Centroid",aggr);
    
    delete centroid;
    
    essentia::standard::Algorithm * mean =essentia::standard::AlgorithmFactory::create("Mean");
    mean->input("array").set(v);
    mean->output("mean").set(aggr);
    mean->compute();
    p->add(name+".Mean",aggr);
    
    delete mean;
    essentia::standard::Algorithm * median =essentia::standard::AlgorithmFactory::create("Median");
    median->input("array").set(v);
    median->output("median").set(aggr);
    median->compute();
    p->add(name+".Median",aggr);
    
    delete median;
    
    essentia::standard::Algorithm * derivSFX =essentia::standard::AlgorithmFactory::create("DerivativeSFX");
    derivSFX->input("envelope").set(v);
    derivSFX->output("derAvAfterMax").set(aggr);
    derivSFX->output("maxDerBeforeMax").set(aggr2);
    derivSFX->compute();
    p->add(name+".derAvAfterMax",aggr);
    p->add(name+".maxDerBeforeMax",aggr2);
    
    
    delete derivSFX;
    

    
    
    
                          };