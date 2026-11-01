#include "../Source/Engine.h"
#include <cstdio>
int main(){ for(double s: {0.0, 44100.0, 96000.0}){ try{ auto b=std::make_unique<p50::Band>(); b->prepare(s); printf("ok %g size %zu\n",s,sizeof(p50::Band)); }catch(std::exception&e){printf("fail %g %s\n",s,e.what());} } }
