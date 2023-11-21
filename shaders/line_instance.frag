#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec4 p1;
layout(location = 2) in vec4 p2;

layout(location = 0) out vec4 outColor;

float Line(in vec2 p, in vec2 a, in vec2 b) {
	vec2 ba = b - a;
	vec2 pa = p - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0., 1.);
	return length(pa - h * ba);
}

float Fill(float sdf) {
	//return step(0, -sdf);
	return clamp(0.5 - sdf / fwidth(sdf), 0, 1);		//Anti Aliased
}

float Circle( vec2 p, float r )
{
    return length(p) - r;
}

float Stroke( float sdf, float width) {
	return Fill(abs(sdf) - width);
}

float Difference(float sdf1, float sdf2) {
	return max(sdf1, -sdf2);
}

float Union(float sdf1, float sdf2) {
	return min(sdf1, sdf2);
}

float Intersection(float sdf1, float sdf2) {
	return max(sdf1, sdf2);
}

float Trapezoid( in vec2 p, in vec2 a, in vec2 b, in float ra, float rb )
{
    float rba  = rb-ra;
    float baba = dot(b -a, b - a);
    float papa = dot(p -a, p - a);
    float paba = dot(p -a, b - a) / baba;
    float x = sqrt( papa - paba * paba * baba );
    float cax = max(0.0,x - ((paba<0.5) ? ra : rb));
    float cay = abs(paba - 0.5) - 0.5;
    float k = rba * rba + baba;
    float f = clamp( (rba*(x-ra) + paba * baba) / k, 0.0, 1.0 );
    float cbx = x-ra - f*rba;
    float cby = paba - f;
    float s = (cbx < 0.0 && cay < 0.0) ? -1.0 : 1.0;
    return s*sqrt( min(cax*cax + cay*cay*baba, cbx*cbx + cby*cby*baba) );
}

float cro(in vec2 a, in vec2 b ) { return a.x*b.y - a.y*b.x; }

float UnevenCapsule( in vec2 p, in vec2 pa, in vec2 pb, in float ra, in float rb )
{
    p  -= pa;
    pb -= pa;
    float h = dot(pb,pb);
    vec2  q = vec2( dot(p,vec2(pb.y,-pb.x)), dot(p,pb) )/h;
    
    //-----------
    
    q.x = abs(q.x);
    
    float b = ra-rb;
    vec2  c = vec2(sqrt(h-b*b),b);
    
    float k = cro(c,q);
    float m = dot(c,q);
    float n = dot(q,q);
    
         if( k < 0.0 ) return sqrt(h*(n            )) - ra;
    else if( k > c.x ) return sqrt(h*(n+1.0-2.0*q.y)) - rb;
                       return m                       - ra;
}

float OrientedBox( in vec2 p, in vec2 a, in vec2 b, float th )
{
    float l = length(b-a);
    vec2  d = (b-a)/l;
    vec2  q = (p-(a+b)*0.5);
          q = mat2(d.x,-d.y,d.y,d.x)*q;
          q = abs(q)-vec2(l,th)*0.5;
    return length(max(q,0.0)) + min(max(q.x,q.y),0.0);    
}

void main() {

	//Line sdf seems to give perfectly fine results, UnevenCapsule would be more accurate though if widths change drastically over the course of the ribbon.
	float line_sdf = Line(gl_FragCoord.xy, p1.xy, p2.xy) - p2.w; 
	//float line_sdf = OrientedBox(gl_FragCoord.xy, p1.xy, p2.xy, p1.w); 
	//float trap_sdf = UnevenCapsule(gl_FragCoord.xy, p1.xy, p2.xy, p1.w * .95, p2.w * .95); 
	float brightness = Fill(line_sdf);

	outColor = fragColor * brightness;
	outColor.rgb *= fragColor.a;
	
}