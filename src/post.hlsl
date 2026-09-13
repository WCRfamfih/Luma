cbuffer Post : register(b0) {
    float4 viewport; // size, size, bloom strength, spread
    float4 tintHeat; // tint rgb, heat strength
    float4 controls; // interior accumulation ceiling, opacity, AA strength
};
Texture2D<float4> source : register(t0);
Texture2D<float4> bloom : register(t1);
SamplerState linearClamp : register(s0);
struct Full { float4 position:SV_POSITION; float2 uv:TEXCOORD0; };
Full FullVS(uint id:SV_VertexID){Full o;o.uv=float2((id<<1)&2,id&2);o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
float4 displayOutput(float4 c,float2 pixel) {
    if(controls.w<0)return c; // Native diagnostic A/B only.
    // Dither only at the final 8-bit boundary. A fixed spatial pattern avoids
    // animated noise; alpha and premultiplied RGB receive the same coverage.
    // Exact zero stays transparent, including outside the bloom footprint.
    float noise=frac(52.9829189*frac(dot(floor(pixel),float2(.06711056,.00583715))))-.5;
    float a=saturate(c.a+noise/255.0);
    return c.a>0?float4(min(c.rgb/max(c.a,.000001),1)*a,a):0;
}
float2 energyDensity(float2 uv){
    float3 e=source.SampleLevel(linearClamp,uv,0).rgb;
    // Undo the material emission gains to measure overlapping coverage.
    // A lone particle contributes at most one, irrespective of its color,
    // point size, crease heat or the audio-driven Bloom multiplier.
    return float2(e.x+controls.x*(1-exp(-e.y/max(.01,controls.x)))+1.8*(1-exp(-e.z/1.8)),e.x/3.2+e.y/2.1);
}
float4 ExtractPS(Full i):SV_TARGET {
    float2 px=.5/viewport.xy;float2 gathered=0;
    [unroll]for(int y=-1;y<=1;y+=2)[unroll]for(int x=-1;x<=1;x+=2)gathered+=energyDensity(i.uv+float2(x,y)*px);
    gathered*=.25;float e=max(0,gathered.x-.45);
    // Bound scatter energy before filtering; retain raw density separately
    // for white-hot material. More overlapping dots must not flood the air.
    return float4(2*(1-exp(-e/2)),gathered.y,0,0);
}
// Dense separable stretched-exponential PSF: covers an area continuously,
// instead of leaving the sparse annular tap pattern visible around highlights.
// The 1.65 exponent preserves a tighter shoulder and accelerating tail decay.
float scatterAxis(float2 uv,float2 axis){
    float total=0,weight=0;
    float stride=3*viewport.w;
    float footprint=log2(max(1,stride*.5));
    [unroll]for(int j=-8;j<=8;++j){
        float w=exp(-6*pow(abs(j)/8.0,1.65));
        total+=source.SampleLevel(linearClamp,uv+axis*j*stride/viewport.xy,footprint).r*w;
        weight+=w;
    }
    return total/weight;
}
float4 ScatterHPS(Full i):SV_TARGET {
    return float4(scatterAxis(i.uv,float2(1,0)),source.SampleLevel(linearClamp,i.uv,0).g,0,0);
}
float4 ScatterPS(Full i):SV_TARGET {
    return float4(scatterAxis(i.uv,float2(0,1)),source.SampleLevel(linearClamp,i.uv,0).g,0,0);
}
float4 CompositePS(Full i):SV_TARGET {
    float3 accum=source.SampleLevel(linearClamp,i.uv,0).rgb;
    float centre=controls.x*(1-exp(-accum.y/max(.01,controls.x)));
    // A separate, bounded crease channel lets narrow folds heat up without
    // turning the whole veil into an opaque disc.
    float core=accum.x+centre+1.8*(1-exp(-accum.z/1.8));
    float2 light=bloom.SampleLevel(linearClamp,i.uv,0).rg;
    float haze=pow(max(0,light.x),1.25)*viewport.z*2;
    float density=accum.x/3.2+accum.y/2.1;
    float heat=1-exp(-max(0,density-1.05)*tintHeat.w*1.1);
    float shoulderHeat=1-exp(-max(0,light.y-1.05)*tintHeat.w*1.1);
    float3 color=lerp(tintHeat.rgb,float3(1,1,1),heat);
    // Air is translucent even on a strong onset. Composite it behind the
    // particle core instead of tone-mapping both into one opaque blur band.
    float coreAlpha=1-exp(-core),haloAlpha=.60*(1-exp(-haze));
    float a=coreAlpha+haloAlpha*(1-coreAlpha);
    float3 rgb=color*coreAlpha+lerp(tintHeat.rgb,1,shoulderHeat)*haloAlpha*(1-coreAlpha);
    float4 result=float4(saturate(rgb),a);
    return controls.z>0?result:displayOutput(result,i.position.xy);
}
float edgeLuma(float4 c){return dot(c.rgb,float3(.299,.587,.114))*.7+c.a*.3;}
float4 FxaaPS(Full i):SV_TARGET {
    // Compact FXAA directional reconstruction, tuned for transparent points.
    // Alpha participates in contrast detection; all filtering is premultiplied.
    float2 p=1/viewport.xy;
    float4 centre=source.SampleLevel(linearClamp,i.uv,0);
    float nw=edgeLuma(source.SampleLevel(linearClamp,i.uv+float2(-1,-1)*p,0));
    float ne=edgeLuma(source.SampleLevel(linearClamp,i.uv+float2(1,-1)*p,0));
    float sw=edgeLuma(source.SampleLevel(linearClamp,i.uv+float2(-1,1)*p,0));
    float se=edgeLuma(source.SampleLevel(linearClamp,i.uv+float2(1,1)*p,0));
    float m=edgeLuma(centre),lo=min(m,min(min(nw,ne),min(sw,se))),hi=max(m,max(max(nw,ne),max(sw,se)));
    if(hi-lo<max(.025,hi*.125))return displayOutput(centre,i.position.xy);
    float2 direction=float2(-(nw+ne-sw-se),nw+sw-ne-se);
    float reduction=max((nw+ne+sw+se)*(.25/8),1.0/128);
    direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduction),-6,6)*p;
    float4 a=.5*(source.SampleLevel(linearClamp,i.uv-direction/6,0)+source.SampleLevel(linearClamp,i.uv+direction/6,0));
    float4 b=a*.5+.25*(source.SampleLevel(linearClamp,i.uv-direction*.5,0)+source.SampleLevel(linearClamp,i.uv+direction*.5,0));
    float lb=edgeLuma(b);float4 reconstructed=lb<lo||lb>hi?a:b;
    // An isolated bright grain is intentional detail, not a polygon edge.
    float n=edgeLuma(source.SampleLevel(linearClamp,i.uv+float2(0,-1)*p,0));
    float s=edgeLuma(source.SampleLevel(linearClamp,i.uv+float2(0,1)*p,0));
    float e=edgeLuma(source.SampleLevel(linearClamp,i.uv+float2(1,0)*p,0));
    float w=edgeLuma(source.SampleLevel(linearClamp,i.uv+float2(-1,0)*p,0));
    float threshold=max(.035,hi*.35);
    float connected=step(threshold,nw)+step(threshold,ne)+step(threshold,sw)+step(threshold,se)+
        step(threshold,n)+step(threshold,s)+step(threshold,e)+step(threshold,w);
    float blend=controls.z*.65*smoothstep(1.5,4.5,connected);
    return displayOutput(lerp(centre,reconstructed,blend),i.position.xy);
}
