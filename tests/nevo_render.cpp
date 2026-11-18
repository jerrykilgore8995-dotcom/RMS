#include "../src/Engine.hpp"
#include <cstdio>
#include <cstdlib>
#include <string_view>
int main(int argc,char**argv){
 if(argc!=3)return 2;
 rmx::Engine e;e.prepare(48000);
 rmx::Settings s;s.program=std::string_view(argv[1])=="ambience"?rmx::ambience:rmx::nonlin2;
 s.decay[0]=std::strtof(argv[2],nullptr); e.set(s);
 for(int i=0;i<48000*5+e.latencySamples();++i){
 auto y=e.process(i==0?.8f:0,i==0?.8f:0);
 if(i>=e.latencySamples()) std::fwrite(y.data(),sizeof(float),2,stdout);
 }
}
