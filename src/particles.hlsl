cbuffer Scene : register(b0) {
    float4 view; // width, height, time, energy
    float4 tint; // linear-ish rgb, opacity
    float4 options; // point size, motion, glow, mode
    float4 grid; // columns, rows, sensitivity, wave-to-limb coupling
    float4 rhythm; // spring displacement, transient, bass, dominant region
    float4 interior; // density, opacity, streamline strength, wave force
    float4 waveSettings; // propagation speed, physical freedom, inner bounce, AA
    float4 rotation; // true XYZ orientation
    float4 atmosphere; // wave irregularity, bloom, spread, heat
    float4 waves[4]; // age, power
    float4 kicks[4]; // independent drum impulse history, age / power
    float4 bands[16];
    float4 framing; // padded render size; zero retains logical-size diagnostics
};
// Shared concept with the settings preview: a translucent, ordered particle veil.
// Four finite-lived sub-triggered wavefronts travel from y=+1.1 to y=-1.1.
float2 waveAt(float y,float theta,float t) {
    float displacement=0,crest=0;
    [unroll] for(int k=0;k<4;k++) {
        float age=waves[k].x,w=waves[k].y*interior.w*grid.z;
        float front=1.12-age*waveSettings.x*1.65;
        float phase=theta*2+t*.25+k*1.71;
        float curl=(sin(phase)*.14+sin(theta*5+age*.8+k)*.075+sin(theta*9-k)*.025)*atmosphere.x;
        curl*=sqrt(saturate(1-y*y));
        float width=lerp(1, .5+.9*pow(.5+.5*sin(theta*4+k*2.1+t*.16),2),saturate(atmosphere.x));
        float d=(y-front+curl)/width;
        float branch=(y-front+curl+.16*sin(theta*6+k)*sqrt(saturate(1-y*y)))/max(.35,width*.5);
        float alive=1-smoothstep(1.7,2.2,age*waveSettings.x);
        float driven=sin(d*10)*exp(-d*d*13)*w*.13*alive;
        float localAge=age-max(0,(1.12-y))/(waveSettings.x*1.65);
        // Closed-form underdamped spring after the wave hits this particle.
        // Captured impulse is the only audio input: return motion survives silence.
        float omega=10.5+sin(theta*3)*1.5;
        float gamma=4.8-2.3*waveSettings.y;
        float freeReturn=localAge>0?exp(-gamma*localAge)*sin(omega*localAge)*w*.22:0;
        displacement+=lerp(driven,freeReturn,waveSettings.y);
        crest+=(exp(-d*d*100)+exp(-branch*branch*135)*.4*atmosphere.x)*w*alive*smoothstep(.04,.22,age*waveSettings.x);
    }
    return float2(clamp(displacement,-.3,.3),min(crest,1.6));
}

float3 turn(float3 p,bool inverse) {
    float sx=sin(rotation.x),cx=cos(rotation.x),sy=sin(rotation.y),cy=cos(rotation.y),sz=sin(rotation.z),cz=cos(rotation.z);
    if(inverse){
        p.xy=float2(p.x*cz+p.y*sz,-p.x*sz+p.y*cz);
        p.xz=float2(p.x*cy-p.z*sy,p.x*sy+p.z*cy);
        p.yz=float2(p.y*cx+p.z*sx,-p.y*sx+p.z*cx);
    }else{
        p.yz=float2(p.y*cx-p.z*sx,p.y*sx+p.z*cx);
        p.xz=float2(p.x*cy+p.z*sy,-p.x*sy+p.z*cy);
        p.xy=float2(p.x*cz-p.y*sz,p.x*sz+p.y*cz);
    }
    return p;
}
// A radial soft body, not a plane and not a fixed-radius sphere. Both the
// visible limb and the veil use this field. The radius bound preserves volume
// through arbitrary rotations while allowing real outward AND inward travel.
float bodyRadius(float3 n,float2 wave,float t) {
    float spring=clamp(rhythm.x*grid.z,-.3,1.25);
    float broad=sin(n.x*3+n.y*1.8+t*.31)*cos(n.z*2.8-n.y*2.1-t*.27);
    float detail=sin(n.x*4.6-n.y*2.4+n.z*1.6+t*.19)*cos(n.y*3.4+n.z*2.2-t*.23);
    // Shape evolves independently. Music changes overall scale. Wave coupling
    // is a third, explicit control (20% by default), not the shape generator.
    return clamp((.785+.044*broad+.012*detail+wave.x*.95*grid.w)*(1+.10*spring),.62,.94);
}
float bounceAt(float seed){
    float displacement=0,variation=frac(sin(seed*91.7+23.6)*17341.71);
    [unroll]for(int k=0;k<4;k++){
        float age=kicks[k].x-variation*.055,power=sqrt(saturate((kicks[k].y-.02)*2.5))*grid.z;
        if(age>0){
            float driven=exp(-8.5*age)*sin(10.9*age)*.34;
            float freeReturn=exp(-(2.8+variation*1.4)*age)*sin((8.4+variation*4.2)*age)*.34;
            displacement+=lerp(driven,freeReturn,waveSettings.y)*power;
        }
    }
    return clamp(displacement,-.20,.28)*waveSettings.z;
}
struct V { float4 position:SV_POSITION; float2 uv:TEXCOORD0; float4 color:COLOR0;float interiorMask:TEXCOORD1;float crease:TEXCOORD2; };
float hash(float n) { return frac(sin(n*127.1+311.7)*43758.5453); }
V VS(uint vertex:SV_VertexID,uint instance:SV_InstanceID) {
    float2 corners[6]={float2(-1,-1),float2(1,-1),float2(-1,1),float2(-1,1),float2(1,-1),float2(1,1)};
    uint total=(uint)(grid.x*grid.y),rimCount=total/3;
    float t=view.z*options.y,spring=clamp(rhythm.x*grid.z,-.12,1.25),hit=saturate(rhythm.y*grid.z),bass=min(rhythm.z*grid.z,1);
    float theta,r,alpha,pixels,crease=0;float3 pos;
    if(instance>=total){
        // Concentrated material threads at each travelling fold. A fixed
        // extra budget follows the retained wave packets; silence emits none.
        uint perWave=total/8,id=instance-total,k=min(3,id/perWave),index=id%perWave;
        float cols=max(16,floor(grid.x*2*sqrt(max(interior.x,.001))));
        float rows=max(2,floor(perWave/cols)),u=(index%(uint)cols)/cols,v=floor(index/cols)/max(1,rows-1);
        theta=u*6.2831853;float age=waves[k].x,power=waves[k].y*interior.w*grid.z;
        float phase=theta*2+t*.25+k*1.71;
        float curl=(sin(phase)*.14+sin(theta*5+age*.8+k)*.075+sin(theta*9-k)*.025)*atmosphere.x;
        float ribbonWidth=.018+.022*pow(.5+.5*sin(theta*4+k*2.1),2);
        float front=1.12-age*waveSettings.x*1.65,y=front;
        [unroll]for(int iteration=0;iteration<3;iteration++)y=front-curl*sqrt(saturate(1-y*y))+(v-.5)*ribbonWidth;
        float cp=sqrt(saturate(1-y*y));float3 n=float3(sin(theta)*cp,clamp(y,-1,1),cos(theta)*cp);
        float2 wave=waveAt(n.y,theta,t);pos=turn(n*(bodyRadius(n,wave,t)-.004),false);
        float facing=smoothstep(-.08,.35,pos.z),alive=1-smoothstep(1.7,2.2,age*waveSettings.x);
        alpha=power*alive*interior.y*interior.z*tint.a*facing*(.38+.32*sin(v*3.14159265))*smoothstep(.05,.32,cp)*smoothstep(.04,.22,age*waveSettings.x);
        // Broken, tapered creases read as passing cloth folds, not latitude lines.
        alpha*=.12+.88*smoothstep(-.35,.7,sin(theta*2+k*1.7+age*.4)+.35*sin(theta*5-k));
        crease=2;pixels=options.x*(1.05+.22*sin(v*3.14159265))*view.x/600;
        if(abs(y)>=.995||index>=uint(cols*rows)||interior.x<=0||options.w>.5)alpha=0;
    }else if(instance<rimCount){
        float cols=grid.x*2,row=floor(instance/cols),u=((instance%(uint)cols)+frac(row*.6180339))/cols,v=row/ceil(rimCount/cols);
        theta=u*6.2831853;
        float2 direction=float2(cos(theta),sin(theta));float limb=0;float2 rimWave=0;
        // Sample nearby depths to follow the deformed body's apparent limb.
        [unroll]for(int j=-1;j<=1;j++){
            float z=j*.24;float3 n=turn(float3(direction*sqrt(1-z*z),z),true);
            float2 wave=waveAt(n.y,atan2(n.x,n.z),t);
            float candidate=bodyRadius(n,wave,t)*sqrt(1-z*z);
            if(candidate>limb){limb=candidate;rimWave=wave;}
        }
        // Broad material lobes fold INWARD from a stable outer silhouette.
        // Evaluate in body coordinates so their origins follow true 3D turn.
        // Unequal, slowly advected lobes avoid a uniformly thick circular line.
        float3 cloth=turn(float3(direction,0),true);
        float drift=sin(dot(cloth,float3(2.7,-1.8,2.3))+t*.23)
            +.45*sin(dot(cloth,float3(-4.2,3.1,1.7))-t*.17);
        float lobe=pow(smoothstep(-.9,1.15,drift),1.6);
        float width=.020+lobe*(.145+.020*(.5+.5*sin(dot(cloth,float3(4,2,-3))+t*.13)))
            +grid.w*(rimWave.y*.038+abs(rimWave.x)*.22);
        float fold=sin(theta*3+t*.3+v*3)*.002*(1-v);
        r=limb+.012-pow(1-v,1.35)*width+fold;
        if(options.w>.5)r=limb+(v-.5)*.025;
        alpha=(.40+.60*pow(v,.45)+.30*pow(sin(v*3.14159265),2)+rimWave.y*.35*grid.w)*tint.a;
        pixels=options.x*(1.15+.2*hash(instance)+rimWave.y*.20*grid.w)*view.x/600;
        pos=float3(direction*r,0);
    }else{
        uint id=instance-rimCount;
        uint count=max(1,uint((total-rimCount)*interior.x));
        float u=frac(float(id)*.61803398875),v=(float(id)+.5)/count;
        theta=u*6.2831853;float y=clamp(1-2*v,-1,1),cp=sqrt(saturate(1-y*y));
        float3 n=float3(sin(theta)*cp,y,cos(theta)*cp);
        // Equal-area, pole-free sampling with stable micro-jitter. Advect it
        // through a 3D current instead of drawing a longitude/latitude mesh.
        float3 jitter=float3(hash(id*3+1),hash(id*3+2),hash(id*3+3))-.5;
        n=normalize(n+jitter*.025);
        float2 wave=waveAt(n.y,atan2(n.x,n.z),t);
        float3 current=float3(sin(n.y*3.2+n.z*2.4+t*.22),sin(n.z*3.6-n.x*2.1-t*.18),sin(n.x*3.1+n.y*2.7+t*.16));
        current-=n*dot(n,current);
        float materialCrest=wave.y+abs(wave.x)*3;
        n=normalize(n+current*.14*interior.z+(float3(0,1,0)-n*n.y)*wave.x*1.35*interior.z);
        wave=waveAt(n.y,atan2(n.x,n.z),t);
        pos=turn(n*(bodyRadius(n,wave,t)-.004),false);
        float facing=smoothstep(-.10,.38,pos.z);
        float threads=.65+.35*smoothstep(-.4,.8,sin(n.x*4+n.z*3+t*.15)+.35*cos(n.y*5-n.x*2));
        alpha=(.82+.38*facing)*threads*interior.y*tint.a;
        alpha*=lerp(.10,1,facing);
        crease=materialCrest*interior.z*facing*.5;
        pixels=options.x*(.80+.28*facing+crease*.10)*view.x/600;
        // Independent kick springs: the retained drum impulse starts a local
        // flight immediately, without waiting for the travelling wavefront.
        // Screen-depth falloff keeps the flights inside the luminous boundary.
        float mobility=lerp(.22,1,smoothstep(.45,.85,hash(id)));
        float flight=bounceAt(float(id)+17)*mobility*pow(saturate(pos.z/.65),2);
        pos+=turn(float3(sin(n.x*5+n.y*2)*.38,1,cos(n.z*4)*.24),false)*flight;
        if(options.w>.5){pos=float3(float2(cos(u*6.2831853),sin(u*6.2831853))*(.765-max(0,spring)*v*.12),0);alpha*=.35;crease=0;}
        if(id>=count||interior.x<=0)alpha=0;
    }
    float2 uv=corners[vertex];
    // Include the pixel filter's support outside the original sprite bounds;
    // otherwise a subpixel dot can disappear before its shader is invoked.
    float support=pixels+(waveSettings.w>0?.75:0);
    float canvas=framing.x>0?framing.x:view.x;
    V o;o.position=float4(pos.xy*.88*view.x/canvas+(uv*support+float2(rotation.w,0))*2/canvas,0,1);o.uv=uv*support/max(.001,pixels);
    o.color=float4(tint.rgb,alpha);o.interiorMask=instance>=rimCount?1:0;o.crease=crease;return o;
}
float particleProfile(float2 uv){
    float d=length(uv);float core=1-smoothstep(0.13,0.54,d);
    float halo=exp(-d*d*5)*options.z*0.35;
    return max(abs(uv.x),abs(uv.y))<=1?core+halo:0;
}
float4 PS(V input):SV_TARGET {
    float coverage=particleProfile(input.uv);
    if(waveSettings.w>0){
        // Four quadrature samples within this pixel, no extra geometry draw.
        // Integrate BEFORE HDR accumulation and tone mapping, preserving the
        // energy of small moving points instead of blurring the finished image.
        float2 dx=ddx(input.uv)*.25,dy=ddy(input.uv)*.25;
        float filtered=(particleProfile(input.uv-dx-dy)+particleProfile(input.uv+dx-dy)+
            particleProfile(input.uv-dx+dy)+particleProfile(input.uv+dx+dy))*.25;
        coverage=lerp(coverage,filtered,waveSettings.w);
    }
    float alpha=saturate(coverage*input.color.a);
    return float4(alpha*(1-input.interiorMask)*3.2,alpha*input.interiorMask*2.1,
        alpha*input.interiorMask*input.crease*7,0);
}
