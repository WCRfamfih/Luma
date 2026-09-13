#pragma once
#include <algorithm>
#include <cmath>
// Uses the general onset history, independent of sub-bass travelling waves.
// Maximum (rather than sum) bounds dense drum rolls to one breathing envelope.
inline float bloomBreath(const float* kicks,float sensitivity,float decay){
    decay=std::clamp(decay,.1f,1.5f);
    constexpr float attack=.018f;
    float peakTime=attack*std::log1p(decay/attack);
    float normalizer=(1-std::exp(-peakTime/attack))*std::exp(-peakTime/decay);
    float envelope=0;
    for(int k=0;k<4;++k){
        float age=kicks[k*4],power=std::sqrt(std::clamp((kicks[k*4+1]-.02f)*2.5f,0.f,1.f));
        float amplitude=std::clamp(power*sensitivity,0.f,1.f);
        if(age>=0)envelope=std::max(envelope,amplitude*(1-std::exp(-age/attack))*std::exp(-age/decay)/normalizer);
    }
    return std::clamp(envelope,0.f,1.f);
}
inline bool bloomBreathSelfTest(){
    float kicks[16]{};if(bloomBreath(kicks,1,.35f)!=0)return false;
    kicks[1]=1;kicks[0]=.05f;float peak=bloomBreath(kicks,1,.35f);
    kicks[0]=.25f;float returning=bloomBreath(kicks,1,.35f);
    kicks[0]=2;float settled=bloomBreath(kicks,1,.35f);
    kicks[0]=.2f;float fast=bloomBreath(kicks,1,.1f),slow=bloomBreath(kicks,1,1.f);
    for(int k=0;k<4;++k){kicks[k*4]=.05f;kicks[k*4+1]=1;}
    return peak>.95f&&returning>.3f&&returning<peak&&settled<.01f&&slow>fast&&bloomBreath(kicks,5,.35f)<=1;
}
