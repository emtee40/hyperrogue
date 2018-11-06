// Hyperbolic Rogue -- hyperbolic graphics
// Copyright (C) 2011-2018 Zeno Rogue, see 'hyper.cpp' for details

namespace hr {

ld ghx, ghy, ghgx, ghgy;
hyperpoint ghpm = C0;

void ghcheck(hyperpoint &ret, const hyperpoint &H) {
  if(hypot(ret[0]-ghx, ret[1]-ghy) < hypot(ghgx-ghx, ghgy-ghy)) {
    ghpm = H; ghgx = ret[0]; ghgy = ret[1];
    }
  }

void camrotate(ld& hx, ld& hy) {
  ld cam = vid.camera_angle * M_PI / 180;
  GLfloat cc = cos(cam);
  GLfloat ss = sin(cam);
  ld ux = hx, uy = hy * cc + ss, uz = cc - ss * hy;
  hx = ux / uz, hy = uy / uz;
  }

hyperpoint perspective_to_space(hyperpoint h, ld alpha = vid.alpha, eGeometryClass geo = ginf[geometry].cclass);

hyperpoint dhp(ld x, ld y, ld z) { return hpxyz(x, y, z); }

hyperpoint perspective_to_space(hyperpoint h, ld alpha, eGeometryClass gc) {
  ld hx = h[0], hy = h[1];
  
  if(gc == gcEuclid)
    return hpxy(hx * (1 + alpha), hy * (1 + alpha));
    
  ld hr = hx*hx+hy*hy;
  
  if(hr > .9999 && gc == gcHyperbolic) return Hypc;
  
  ld A, B, C;
  
  ld curv = gc == gcSphere ? 1 : -1;
  
  A = 1+curv*hr;
  B = 2*hr*vid.alpha*-curv;
  C = 1 - curv*hr*vid.alpha*vid.alpha;
  
  B /= A; C /= A;
  
  ld rootsign = 1;
  if(gc == gcSphere && vid.alpha > 1) rootsign = -1;
  
  ld hz = B / 2 + rootsign * sqrt(C + B*B/4);
  
  hyperpoint H;
  H[0] = hx * (hz+vid.alpha);
  H[1] = hy * (hz+vid.alpha);
  H[2] = hz;
  
  return H;  
  }

hyperpoint space_to_perspective(hyperpoint z, ld alpha = vid.alpha);

hyperpoint space_to_perspective(hyperpoint z, ld alpha) {
  ld s = 1 / (alpha + z[2]);
  z[0] *= s;
  z[1] *= s;
  z[2] = 0;
  return z;
  }

hyperpoint gethyper(ld x, ld y) {

  ld hx = (x - vid.xcenter) / vid.radius;
  ld hy = (y - vid.ycenter) / vid.radius / vid.stretch;
  
  if(pmodel) {
    ghx = hx, ghy = hy;
    return ghpm;
    }
  
  if(vid.camera_angle) camrotate(hx, hy);
  
  return perspective_to_space(hpxyz(hx, hy, 0));
  }

void ballmodel(hyperpoint& ret, double alpha, double d, double zl) {
  hyperpoint H = ypush(geom3::camera) * xpush(d) * ypush(zl) * C0;
  ld tzh = vid.ballproj + H[2];
  ld ax = H[0] / tzh;
  ld ay = H[1] / tzh;
  
  ld ca = cos(alpha), sa = sin(alpha);

  ret[0] = ax * ca;
  ret[1] = ay;
  ret[2] = ax * sa;
  
  conformal::apply_ball(ret[2], ret[1]);
  }

void apply_depth(hyperpoint &f, ld z) {
  if(vid.usingGL)
    f[2] = z;
  else {
    z = z * vid.radius;
    ld mul = stereo::scrdist / (stereo::scrdist + z);
    f[0] = f[0] * mul;
    f[1] = f[1] * mul;
    f[2] = vid.xres * stereo::eyewidth() / 2 / vid.radius + stereo::ipd * mul / 2;
    }
  }

bool hypot_zlev(ld zlev, ld& d, ld& df, ld& zf) {
  if(zlev == 1) {
    df = 1; zf = 0;
    return false;
    }
  else {
    // (0,0,1) -> (0, sin z, cos z) -> (sin d cos z, sin z, cos d cos z)
    ld z = geom3::factor_to_lev(zlev);
    
    ld tz = sin_auto(z);
    ld td = sin_auto(abs(d)) * cos_auto(z);
    ld h = hypot(td, tz);
    zf = tz / h, df = td / h;

    if(d > 0)
      d = hypot_auto(d, z);
    else
      d = -hypot_auto(d, z);
    return true;
    }
  }

int twopoint_sphere_flips;
bool twopoint_do_flips;

ld find_zlev(hyperpoint& H) {

  if(wmspatial || mmspatial) {
    ld zlev = zlevel(H);
    using namespace hyperpoint_vec;
    if(zlev > 1-1e-6 && zlev < 1+1e-6) return 1;
    H /= zlev;
    return zlev;
    }  
  
  return 1;
  }

ld get_tz(hyperpoint H) {
  ld tz = euclid ? (1+vid.alpha) : vid.alpha+H[2];
  if(tz < BEHIND_LIMIT && tz > -BEHIND_LIMIT) tz = BEHIND_LIMIT;
  return tz;
  }

ld atan2(hyperpoint h) {
  return atan2(h[1], h[0]);
  }

template<class T> void makeband(hyperpoint H, hyperpoint& ret, const T& f) {
  ld zlev = find_zlev(H);
  conformal::apply_orientation(H[0], H[1]);
  
  ld x, y, yf, zf=0;
  y = asin_auto(H[1]);
  x = asin_auto_clamp(H[0] / cos_auto(y));
  if(sphere) {
    if(H[2] < 0 && x > 0) x = M_PI - x;
    else if(H[2] < 0 && x <= 0) x = -M_PI - x;
    }
  hypot_zlev(zlev, y, yf, zf);
  
  f(x, y);
  
  ld yzf = y * zf; y *= yf;
  conformal::apply_orientation(y, x);
  ret = hpxyz(x / M_PI, y / M_PI, 0);
  if(zlev != 1 && stereo::active()) 
    apply_depth(ret, yzf / M_PI);
  return;
  }

void band_conformal(ld& x, ld& y) {
  switch(cgclass) {
    case gcSphere:
      y = atanh(sin(y));
      x *= 2; y *= 2;
      break;
    case gcHyperbolic:
      y = 2 * atan(tanh(y/2));
      x *= 2; y *= 2;
      break;
    case gcEuclid:
      // y = y;
      y *= 2; x *= 2;
      break;
    }
  }

void make_twopoint(ld& x, ld& y) {
  auto p = vid.twopoint_param;
  ld dleft = hypot_auto(x-p, y);
  ld dright = hypot_auto(x+p, y);
  if(sphere) {
    int tss = twopoint_sphere_flips;
    if(tss&1) { tss--; 
      dleft = 2*M_PI - 2*p - dleft;
      dright = 2*M_PI - 2*p - dright;
      swap(dleft, dright);
      y = -y;
      }
    while(tss) { tss -= 2;
      dleft = 2*M_PI - 4*p + dleft;
      dright = 2*M_PI - 4*p + dright;
      }
    }
  x = (dright*dright-dleft*dleft) / 4 / p;
  y = (y>0?1:-1) * sqrt(dleft * dleft - (x-p)*(x-p) + 1e-9);
  }

void applymodel(hyperpoint H, hyperpoint& ret) {
  
  using namespace hyperpoint_vec;
  
  switch(pmodel) {
    case mdUnchanged:
      ret = H / vid.radius;
      return; 
    
    case mdBall: {
      ld zlev = find_zlev(H);
      
      ld zl = geom3::depth-geom3::factor_to_lev(zlev);
  
      ballmodel(ret, atan2(H), hdist0(H), zl);
      break;      
      }
    
    case mdDisk: {
      ld tz = get_tz(H);
      if(!vid.camera_angle) {
        ret[0] = H[0] / tz;
        ret[1] = H[1] / tz;
        ret[2] = vid.xres * stereo::eyewidth() / 2 / vid.radius - stereo::ipd / tz / 2;
        }
      else {
        ld tx = H[0];
        ld ty = H[1];
        ld cam = vid.camera_angle * M_PI / 180;
        GLfloat cc = cos(cam);
        GLfloat ss = sin(cam);
        ld ux = tx, uy = ty * cc - ss * tz, uz = tz * cc + ss * ty;
        ret[0] = ux / uz;
        ret[1] = uy / uz;
        ret[2] = vid.xres * stereo::eyewidth() / 2 / vid.radius - stereo::ipd / uz / 2;
        }
      return;
      }
    
    case mdHalfplane: {
      // Poincare to half-plane
      
      ld zlev = find_zlev(H);
      H = space_to_perspective(H);
      
      conformal::apply_orientation(H[0], H[1]);
  
      H[1] += 1;
      double rad = sqhypot2(H);
      H /= -rad;
      H[1] += .5;
      
      conformal::apply_orientation(H[0], H[1]);
      
      H *= conformal::halfplane_scale;
      
      ret[0] = -conformal::osin - H[0];
      if(zlev != 1) {
        if(abs(conformal::ocos) > 1e-5)
          H[1] = H[1] * pow(zlev, conformal::ocos);
        if(abs(conformal::ocos) > 1e-5 && conformal::osin)
          H[1] += H[0] * conformal::osin * (pow(zlev, conformal::ocos) - 1) / conformal::ocos;
        else if(conformal::osin)
          H[1] += H[0] * conformal::osin * log(zlev);
        }
      ret[1] = conformal::ocos + H[1];
      ret[2] = 0;
      if(zlev != 1 && stereo::active()) 
        apply_depth(ret, -H[1] * geom3::factor_to_lev(zlev));
      break;
      }
    
    case mdHemisphere: {
  
      switch(cgclass) {
        case gcHyperbolic: {
          ld zl = zlevel(H);
          ret = H / H[2];
          ret[2] = sqrt(1 - sqhypot2(ret));
          ret = ret * (1 + (zl - 1) * ret[2]);
          break;
          }
          
        case gcEuclid: {
          // stereographic projection to a sphere
          auto hd = hdist0(H) / vid.euclid_to_sphere;
          if(hd == 0) ret = hpxyz(0, 0, -1);
          else {
            ld x = 2 * hd / (1 + hd * hd);
            ld y = x / hd;
            ret = H * x / hd / vid.euclid_to_sphere;
            ret[2] = (1 - y);
            ret = ret * (1 + (H[2]-1) * y / vid.euclid_to_sphere);
            }
          break;
          }
        
        case gcSphere: {
          ret = H;
          break;
          }
        }
      
      swap(ret[1], ret[2]);
      
      conformal::apply_ball(ret[2], ret[1]);
      
      break;
      }
    
    case mdHyperboloidFlat: 
    case mdHyperboloid: {
    
      if(pmodel == mdHyperboloid) {
        ld& topz = conformal::top_z;
        if(H[2] > topz) {
          ld scale = sqrt(topz*topz-1) / hypot2(H);
          H *= scale;
          H[2] = topz;
          }
        }
      else {
        H = space_to_perspective(H, vid.alpha);
        H[2] = 1 - vid.alpha;
        }
  
      ret[0] = H[0] / 3;
      ret[1] = (1 - H[2]) / 3;
      ret[2] = H[1] / 3;
      
      conformal::apply_ball(ret[2], ret[1]);
      break;
      }
    
    case mdFisheye: {
      ld zlev = find_zlev(H);
      H = space_to_perspective(H);
      H[2] = zlev;
      ret = H / sqrt(1 + sqhypot3(H));
      break;
      }
    
    case mdJoukowsky: 
    case mdJoukowskyInverted: {
      conformal::apply_orientation(H[0], H[1]);
      // with equal speed skiprope: conformal::apply_orientation(H[1], H[0]);
  
      if(vid.skiprope) {
        static ld last_skiprope = 0;
        static transmatrix lastmatrix;
        if(vid.skiprope != last_skiprope) {
          hyperpoint r = hpxyz(0, 0, 1);
          hyperpoint h = perspective_to_space(r, 1, gcSphere);
          hyperpoint h1 = rotmatrix(-vid.skiprope * M_PI / 180, 1, 2) * h;
          hyperpoint ret = space_to_perspective(h1, 1) / 2;
          typedef complex<ld> cld;
          const cld c1(1, 0);
          const cld c2(2, 0);
          const cld c4(4, 0);
          cld w(ret[0], ret[1]);
          cld z = sqrt(c4*w*w-c1) + c2*w;
          if(abs(z) > 1) z = c1 / z;
          hyperpoint zr = hpxyz(real(z), imag(z), 0);
          
          hyperpoint inhyp = perspective_to_space(zr, 1, gcHyperbolic);
          last_skiprope = vid.skiprope;
          lastmatrix = rgpushxto0(inhyp);
          }
        H = lastmatrix * H;
        }
  
      H = space_to_perspective(H);
      ld r = hypot2(H);
      ld c = H[0] / r;
      ld s = H[1] / r;
      ld& mt = conformal::model_transition;
      ld a = 1 - .5 * mt, b = .5 * mt;
      swap(a, b);
      
      ret[0] = (a * r + b/r) * c / 2;
      ret[1] = (a * r - b/r) * s / 2;
      ret[2] = 0;

      if(vid.skiprope) {
        hyperpoint h = perspective_to_space(ret * 2, 1, gcSphere);
        h = rotmatrix(vid.skiprope * M_PI / 180, 1, 2) * h;
        ret = space_to_perspective(h, 1) / 2;
        }
      
      if(pmodel == mdJoukowskyInverted) {
        ld r2 = sqhypot2(ret);
        ret[0] = ret[0] / r2;
        ret[1] = -ret[1] / r2;
        conformal::apply_orientation(ret[1], ret[0]);
        
        /*
  
        ret[0] += 1;
        ld alpha = atan2(ret[1], ret[0]);
        ld mod = hypot(ret[0], ret[1]);
        // ret[0] = cos(alpha/2) * sqrt(mod);
        // ret[1] = sin(alpha/2) * sqrt(mod);
        ret[0] = alpha;
        ret[1] = log(mod); */
        }
      else conformal::apply_orientation(ret[0], ret[1]);
  
      break;
      }
    
    case mdPolygonal: case mdPolynomial: {
    
      H = space_to_perspective(H);

      conformal::apply_orientation(H[0], H[1]);
  
      pair<long double, long double> p = polygonal::compute(H[0], H[1]);
  
      conformal::apply_orientation(p.second, p.first);
      ret[0] = p.first;
      ret[1] = p.second;
      ret[2] = 0;
      break;
      }  
      
    case mdBand: 
      if(conformal::model_transition != 1) {
        ld& mt = conformal::model_transition;
        
        H = space_to_perspective(H);
        
        conformal::apply_orientation(H[0], H[1]);
    
        H[0] += 1;
        double rad = H[0]*H[0] + H[1]*H[1];
        H[1] /= rad;
        H[0] /= rad;
        H[0] -= .5;
        
        ld phi = atan2(H);
        ld r = hypot2(H);
        
        r = pow(r, 1 - mt);
        phi *= (1 - mt);
        ret[0] = r * cos(phi);
        ret[1] = r * sin(phi);
        ret[2] = 0;
        
        ret[0] -= pow(0.5, 1-mt);
        ret[0] /= -(1-mt) * M_PI / 2;
        ret[1] /= (1-mt) * M_PI / 2;
        
        conformal::apply_orientation(ret[1], ret[0]);
        }
      else 
        makeband(H, ret, band_conformal);
      break;
      
    case mdTwoPoint: 
      makeband(H, ret, make_twopoint);
      break;
    
    case mdBandEquiarea: 
      makeband(H, ret, [] (ld& x, ld& y) { y = sin_auto(y); });
      break;
    
    case mdBandEquidistant:
      makeband(H, ret, [] (ld& x, ld& y) { });
      break;
    
    case mdSinusoidal: 
      makeband(H, ret, [] (ld& x, ld& y) { x *= cos_auto(y); });
      break;
    
    case mdEquidistant: case mdEquiarea: {
      ld zlev = find_zlev(H);

      ld rad = hypot2(H);
      if(rad == 0) rad = 1;
      ld d = hdist0(H);
      ld df, zf;
      hypot_zlev(zlev, d, df, zf);
      
      // 4 pi / 2pi = M_PI 
      
      if(pmodel == mdEquiarea && sphere)
        d = sqrt(2*(1 - cos(d))) * M_PI / 2;
      else if(pmodel == mdEquiarea && hyperbolic)
        d = sqrt(2*(cosh(d) - 1)) / 1.5;

      ret = H * (d * df / rad / M_PI);
      ret[2] = 0; 
      if(zlev != 1 && stereo::active()) 
        apply_depth(ret, d * zf / M_PI);
      
      break;
      }
    
    case mdGUARD: break;
    }

  ghcheck(ret,H);
  }

// game-related graphics

transmatrix View; // current rotation, relative to viewctr
transmatrix cwtV = Id; // player-relative view
transmatrix sphereflip; // on the sphere, flip
heptspin viewctr; // heptagon and rotation where the view is centered at
bool playerfound; // has player been found in the last drawing?

#define eurad crossf

double q3 = sqrt(double(3));

bool outofmap(hyperpoint h) {
  if(euclid) 
    return h[2] < .5; // false; // h[0] * h[0] + h[1] * h[1] > 15 * eurad;
  else if(sphere)
    return h[2] < .1 && h[2] > -.1 && h[1] > -.1 && h[1] < .1 && h[0] > -.1 && h[0] < .1;
  else
    return h[2] < .5;
  }

hyperpoint mirrorif(const hyperpoint& V, bool b) {
  if(b) return Mirror*V;
  else return V;
  }

transmatrix mirrorif(const transmatrix& V, bool b) {
  if(b) return V*Mirror;
  else return V;
  }

// -1 if away, 0 if not away
int away(const transmatrix& V2) {
  return (intval(C0, V2 * xpush0(.1)) > intval(C0, tC0(V2))) ? -1 : 0;
  }

/* double zgrad(double f1, double f2, int nom, int den) {
  using namespace geom3;
  ld fo1 = factor_to_lev(f1);
  ld fo2 = factor_to_lev(f2);
  return lev_to_factor(fo1 + (fo2-fo1) * nom / den);
  } */

double zgrad0(double l1, double l2, int nom, int den) {
  using namespace geom3;
  return lev_to_factor(l1 + (l2-l1) * nom / den);
  }

bool behindsphere(const hyperpoint& h) {
  if(!sphere) return false;

  if(mdBandAny()) return false;

  if(vid.alpha > 1) {
     if(h[2] > -1/vid.alpha) return true;
     }  
  
  if(vid.alpha <= 1) {
    if(h[2] < .2-vid.alpha) return true;
    }
  
  return false;
  }

ld to01(ld a0, ld a1, ld x) {
  if(x < a0) return 0;
  if(x > a1) return 1;
  return (x-a0) / (a1-a0);
  }

ld spherity(const hyperpoint& h) {
  if(!sphere) return 1;
  
  if(vid.alpha > 1) {
    return to01(1/vid.alpha, 1, -h[2]);
    }  
  
  if(vid.alpha <= 1) {
    return to01(-1.5, 1, h[2]);
    }
      
  return 1;
  }

bool behindsphere(const transmatrix& V) {
  return behindsphere(tC0(V));
  }

ld spherity(const transmatrix& V) {
  return spherity(tC0(V));
  }

bool confusingGeometry() {
  return elliptic || quotient || torus;
  }

ld master_to_c7_angle() {
  return (!BITRUNCATED && !binarytiling && !archimedean) ? M_PI + gp::alpha : 0;
  }

transmatrix actualV(const heptspin& hs, const transmatrix& V) {
  if(IRREGULAR)
    return V * spin(M_PI + 2 * M_PI / S7 * (hs.spin + irr::periodmap[hs.at].base.spin));
  if(archimedean) return V * spin(-arcm::current.triangles[arcm::id_of(hs.at)][hs.spin].first);
  if(binarytiling) return V;
  return (hs.spin || !BITRUNCATED) ? V * spin(hs.spin*2*M_PI/S7 + master_to_c7_angle()) : V;
  }

transmatrix applyspin(const heptspin& hs, const transmatrix& V) {
  if(binarytiling) return V;
  if(archimedean) return V * spin(arcm::current.triangles[arcm::id_of(hs.at)][hs.spin].first);
  return hs.spin ? V * spin(hs.spin*2*M_PI/S7) : V;
  }

bool in_smart_range(const transmatrix& T) {
  hyperpoint h1, h2, h3;
  applymodel(tC0(T), h1);
  ld x = vid.xcenter + vid.radius * h1[0];
  ld y = vid.ycenter + vid.radius * h1[1] * vid.stretch;
  if(x > vid.xres * 2) return false;
  if(x < -vid.xres) return false;
  if(y > vid.yres * 2) return false;
  if(y < -vid.yres) return false;
  ld epsilon = 0.01;
  applymodel(T * xpush0(epsilon), h2);
  ld x1 = vid.radius * abs(h2[0] - h1[0]) / epsilon;
  ld y1 = vid.radius * abs(h2[1] - h1[1]) * vid.stretch / epsilon;
  applymodel(T * ypush(epsilon) * C0, h3);
  ld x2 = vid.radius * abs(h3[0] - h1[0]) / epsilon;
  ld y2 = vid.radius * abs(h3[1] - h1[1]) * vid.stretch / epsilon;
  ld scale = sqrt(hypot(x1, y1) * hypot(x2, y2)) * scalefactor * hcrossf7;
  return 
    scale > vid.smart_range_detail && 
    x - 2 * max(x1, x2) < vid.xres && 
    x + 2 * max(x1, x2) > 0 &&
    y - 2 * max(y1, y2) < vid.yres &&
    y + 2 * max(y1, y2) > 0;
  }

// in hyperbolic quotient geometries, relying on pathdist is not sufficient
bool in_qrange(const transmatrix& V) {
  if(!quotient || !hyperbolic || vid.use_smart_range) return true;
  return V[2][2] < cosh(crossf * get_sightrange_ambush());
  }

namespace gp {

/*
void drawrec(cell *c, const transmatrix& V) {
  if(dodrawcell(c))
    drawcell(c, V, 0, false);
  for(int i=0; i<c->type; i++) {
    cell *c2 = c->move(i);
    if(!c2) continue;
    if(c2->move(0) != c) continue;
    if(c2 == c2->master->c7) continue;
    transmatrix V1 = V * ddspin(c, i) * xpush(crossf) * iddspin(c2, 0) * spin(M_PI);
    drawrec(c2, V1);
    }
  } */
  
  gp::local_info draw_li;

  void drawrec(cell *c, const transmatrix& V, gp::loc at, int dir, int maindir) {
    if(dodrawcell(c)) {
      /* auto li = get_local_info(c);
      if(fix6(dir) != fix6(li.total_dir)) printf("totaldir %d/%d\n", dir, li.total_dir);
      if(at != li.relative) printf("at %s/%s\n", disp(at), disp(li.relative));
      if(maindir != li.last_dir) printf("ld %d/%d\n", maindir, li.last_dir); */
      draw_li.relative = at;
      draw_li.total_dir = fixg6(dir);
      transmatrix V1 = V * Tf[draw_li.last_dir][at.first&31][at.second&31][fixg6(dir)];
      if(in_qrange(V1))
        drawcell(c, V1, 0, false);
      }
    for(int i=0; i<c->type; i++) {
      cell *c2 = c->move(i);
      if(!c2) continue;
      if(c2->move(0) != c) continue;
      if(c2 == c2->master->c7) continue;
      drawrec(c2, V, at + eudir(dir+i), dir + i + SG3, maindir);
      }
    }
  
  void drawrec(cell *c, const transmatrix& V) {
    draw_li.relative = loc(0,0);
    draw_li.total_dir = 0;
    draw_li.last_dir = -1;
    if(dodrawcell(c))
      drawcell(c, V, 0, false);
    for(int i=0; i<c->type; i++) {
      cell *c2 = c->move(i);
      if(!c2) continue;
      if(c2->move(0) != c) continue;
      if(c2 == c2->master->c7) continue;
      draw_li.last_dir = i;
      drawrec(c2, V, gp::loc(1,0), SG3, i);
      }
    }
  }

void drawrec(const heptspin& hs, hstate s, const transmatrix& V, int reclev) {

  // calc_relative_matrix(cwt.c, hs.at);
    
  cell *c = hs.at->c7;
  
  transmatrix V10;
  const transmatrix& V1 = hs.mirrored ? (V10 = V * Mirror) : V;
  
  bool draw = c->pathdist < PINFD;
  
  if(cells_drawn > vid.cells_drawn_limit || reclev >= 100 || std::isinf(V[2][2]) || std::isnan(V[2][2]))
    draw = false;
  else if(vid.use_smart_range) {
    draw = reclev < 2 ? true : in_smart_range(V);
    if(draw && vid.use_smart_range == 2) 
      setdist(c, 7, NULL);
    }
    
  if(GOLDBERG) {
    gp::drawrec(c, actualV(hs, V1));
    }
  
  else if(IRREGULAR) {
    auto& hi = irr::periodmap[hs.at];
    transmatrix V0 = actualV(hs, V1);
    auto& vc = irr::cells_of_heptagon[hi.base.at];
    for(int i=0; i<isize(vc); i++)
      if(dodrawcell(hi.subcells[i]) && in_qrange(V0 * irr::cells[vc[i]].pusher))
        draw = true,
        drawcell(hi.subcells[i], V0 * irr::cells[vc[i]].pusher, 0, false);
    }
  
  else {
    if(dodrawcell(c)) {
      transmatrix V2 = actualV(hs, V1);
      drawcell(c, V2, 0, hs.mirrored);
      }
    
    if(BITRUNCATED) for(int d=0; d<S7; d++) {
      int ds = hs.at->c.fix(hs.spin + d);
      // createMov(c, ds);
      if(c->move(ds) && c->c.spin(ds) == 0 && dodrawcell(c->move(ds))) {
        transmatrix V2 = V1 * hexmove[d];
        if(in_qrange(V2))
        drawcell(c->move(ds), V2, 0, hs.mirrored ^ c->c.mirror(ds));
        }
      }
    }

  if(draw && in_qrange(V)) for(int d=0; d<S7; d++) {
    hstate s2 = transition(s, d);
    if(s2 == hsError) continue;
    heptspin hs2 = hs + d + wstep;
    drawrec(hs2, s2, V * heptmove[d], reclev+1);
    }
  
  }

int mindx=-7, mindy=-7, maxdx=7, maxdy=7;
  
transmatrix eumove(ld x, ld y) {
  transmatrix Mat = Id;
  Mat[2][2] = 1;
  
  if(a4) {
    Mat[0][2] += x * eurad;
    Mat[1][2] += y * eurad;
    }
  else {
    Mat[0][2] += (x + y * .5) * eurad;
    // Mat[2][0] += (x + y * .5) * eurad;
    Mat[1][2] += y * q3 /2 * eurad;
    // Mat[2][1] += y * q3 /2 * eurad;
    }
  
  ld v = a4 ? 1 : q3;

  while(Mat[0][2] <= -16384 * eurad) Mat[0][2] += 32768 * eurad;
  while(Mat[0][2] >= 16384 * eurad) Mat[0][2] -= 32768 * eurad;
  while(Mat[1][2] <= -16384 * v * eurad) Mat[1][2] += 32768 * v * eurad;
  while(Mat[1][2] >= 16384 * v * eurad) Mat[1][2] -= 32768 * v * eurad;
  return Mat;
  }

transmatrix eumove(int vec) {
  int x, y;
  tie(x,y) = vec_to_pair(vec);
  return eumove(x, y);
  }

transmatrix eumovedir(int d) {
  if(a4) {
    d = d & 3;
    switch(d) {
      case 0: return eumove(1,0);
      case 1: return eumove(0,1);
      case 2: return eumove(-1,0);
      case 3: return eumove(0,-1);
      }
    }
  else {
    d = fix6(d);
    switch(d) {
      case 0: return eumove(1,0);
      case 1: return eumove(0,1);
      case 2: return eumove(-1,1);
      case 3: return eumove(-1,0);
      case 4: return eumove(0,-1);
      case 5: return eumove(1,-1);
      }
    }
  return eumove(0,0);
  }

ld matrixnorm(const transmatrix& Mat) {
  return Mat[0][2] * Mat[0][2] + Mat[1][2] * Mat[1][2];
  }
  
void drawEuclidean() {
  DEBB(DF_GRAPH, (debugfile,"drawEuclidean\n"));
  sphereflip = Id;
  if(!centerover.at) centerover = cwt;
  // printf("centerover = %p player = %p [%d,%d]-[%d,%d]\n", lcenterover, cwt.c,
  //   mindx, mindy, maxdx, maxdy);
  int pvec = cellwalker_to_vec(centerover);
    
  int minsx = mindx-1, maxsx=maxdx+1, minsy=mindy-1, maxsy=maxdy+1;
  mindx=maxdx=mindy=maxdy=0;
  
  transmatrix View0 = View;
  
  ld cellrad = vid.radius / (1 + vid.alpha);
  
  ld centerd = matrixnorm(View0);
  
  for(int dx=minsx; dx<=maxsx; dx++)
  for(int dy=minsy; dy<=maxsy; dy++) {
    torusconfig::torus_cx = dx;
    torusconfig::torus_cy = dy;
    cellwalker cw = vec_to_cellwalker(pvec + euclid_getvec(dx, dy));
    transmatrix Mat = eumove(dx,dy);
    
    if(!cw.at) continue;

    Mat = View0 * Mat;
    
    if(true) {
      ld locald = matrixnorm(Mat);
      if(locald < centerd) centerd = locald, centerover = cw, View = View0 * eumove(dx, dy);
      }
    
    // Mat[0][0] = -1;
    // Mat[1][1] = -1;
    
    // Mat[2][0] = x*x/10;
    // Mat[2][1] = y*y/10;
    // Mat = Mat * xpush(x-30) * ypush(y-30);
    
    if(vid.use_smart_range) {
      if(in_smart_range(Mat)) {
        if(dx < mindx) mindx = dx;
        if(dy < mindy) mindy = dy;
        if(dx > maxdx) maxdx = dx;
        if(dy > maxdy) maxdy = dy;
        if(dodrawcell(cw.at))
          drawcell(cw.at, cw.mirrored ? Mat * Mirror : Mat, cw.spin, cw.mirrored);
        }
      }
    else {
      int cx, cy, shift;
      getcoord0(tC0(Mat), cx, cy, shift);
      if(cx >= 0 && cy >= 0 && cx < vid.xres && cy < vid.yres) {
        if(dx < mindx) mindx = dx;
        if(dy < mindy) mindy = dy;
        if(dx > maxdx) maxdx = dx;
        if(dy > maxdy) maxdy = dy;
        }
      if(cx >= -cellrad && cy >= -cellrad && cx < vid.xres+cellrad && cy < vid.yres+cellrad)
        if(dodrawcell(cw.at)) {
          drawcell(cw.at, cw.mirrored ? Mat * Mirror : Mat, cw.spin, cw.mirrored);
          }
      }
    }
  }

void spinEdge(ld aspd) { 
  if(downspin >  aspd) downspin =  aspd;
  if(downspin < -aspd) downspin = -aspd;
  View = spin(downspin) * View;
  }

void centerpc(ld aspd) { 
  if(ors::mode == 2 && vid.sspeed < 5) return;
  if(vid.sspeed >= 4.99) aspd = 1000;
  DEBB(DF_GRAPH, (debugfile,"center pc\n"));
  
  ors::unrotate(cwtV); ors::unrotate(View);
  
  hyperpoint H = ypush(-vid.yshift) * sphereflip * tC0(cwtV);
  ld R = H[0] == 0 && H[1] == 0 ? 0 : hdist0(H); // = sqrt(H[0] * H[0] + H[1] * H[1]);
  if(R < 1e-9) {
    // either already centered or direction unknown
    /* if(playerfoundL && playerfoundR) {
      
      } */
    spinEdge(aspd);
    fixmatrix(View);
    ors::rerotate(cwtV); ors::rerotate(View);
    return;
    }
  
  if(euclid) {
    // Euclidean
    aspd *= (2+3*R*R);
    if(aspd > R) aspd = R;
    
    View[0][2] -= cwtV[0][2] * aspd / R;
    View[1][2] -= cwtV[1][2] * aspd / R;
        
    }
  
  else {
    aspd *= (1+R+(shmup::on?1:0));

    if(R < aspd) {
      View = gpushxto0(H) * View;
      }
    else 
      View = rspintox(H) * xpush(-aspd) * spintox(H) * View;
      
    fixmatrix(View);
    spinEdge(aspd);
    }

  ors::rerotate(cwtV); ors::rerotate(View);
  }

void optimizeview() {
  
  if(centerover.at && inmirror(centerover.at)) {
    anims::reflect_view();
    }
  
  DEBB(DF_GRAPH, (debugfile,"optimize view\n"));
  int turn = 0;
  ld best = INF;
  
  transmatrix TB = Id;
  
  if(binarytiling || archimedean) {
    turn = -1, best = View[2][2];
    for(int i=0; i<viewctr.at->c7->type; i++) {
      int i1 = i * DUALMUL;
      heptagon *h2 = createStep(viewctr.at, i1);
      transmatrix T = (binarytiling) ? binary::relative_matrix(h2, viewctr.at) : arcm::relative_matrix(h2, viewctr.at);
      hyperpoint H = View * tC0(T);
      ld quality = euclid ? hdist0(H) : H[2];
      if(quality < best) best = quality, turn = i1, TB = T;
      }
    if(turn >= 0) {
      View = View * TB;
      fixmatrix(View);
      viewctr.at = createStep(viewctr.at, turn);
      }
    }
  
  else {
  
    for(int i=-1; i<S7; i++) {
  
      ld trot = -i * M_PI * 2 / (S7+.0);
      transmatrix T = i < 0 ? Id : spin(trot) * xpush(tessf) * pispin;
      hyperpoint H = View * tC0(T);
      if(H[2] < best) best = H[2], turn = i, TB = T;
      }
  
    if(turn >= 0) {
      View = View * TB;
      fixmatrix(View);
      viewctr = viewctr + turn + wstep;
      }
    }
  }

void addball(ld a, ld b, ld c) {
  hyperpoint h;
  ballmodel(h, a, b, c);
  for(int i=0; i<3; i++) h[i] *= vid.radius;
  curvepoint(h);
  }

void ballgeometry() {
  queuereset(vid.usingGL ? mdDisk : mdUnchanged, PPR::CIRCLE);
  for(int i=0; i<60; i++)
    addball(i * M_PI/30, 10, 0);
  for(double d=10; d>=-10; d-=.2)
    addball(0, d, 0);
  for(double d=-10; d<=10; d+=.2)
    addball(0, d, geom3::depth);
  addball(0, 0, -geom3::camera);
  addball(0, 0, geom3::depth);
  addball(0, 0, -geom3::camera);
  addball(0, -10, 0);
  addball(0, 0, -geom3::camera);
  queuecurve(darkena(0xFF, 0, 0x80), 0, PPR::CIRCLE);
  queuereset(pmodel, PPR::CIRCLE);
  }

void resetview() {
  DEBB(DF_GRAPH, (debugfile,"reset view\n"));
  View = Id;
  // EUCLIDEAN
  if(!masterless) 
    viewctr.at = cwt.at->master,
    viewctr.spin = cwt.spin;
  else centerover = cwt;
  cwtV = Id;
  // SDL_LockSurface(s);
  // SDL_UnlockSurface(s);
  }


void panning(hyperpoint hf, hyperpoint ht) {
  View = 
    rgpushxto0(hf) * rgpushxto0(gpushxto0(hf) * ht) * gpushxto0(hf) * View;
  playermoved = false;
  }

int cells_drawn;

void fullcenter() {
  if(playerfound && false) centerpc(INF);
  else {
    bfs();
    resetview();
    drawthemap();
    centerpc(INF);
    centerover = cwt.at;
    }
  playermoved = true;
  }

transmatrix screenpos(ld x, ld y) {
  transmatrix V = Id;
  V[0][2] += (x - vid.xcenter) / vid.radius * (1+vid.alpha);
  V[1][2] += (y - vid.ycenter) / vid.radius * (1+vid.alpha);
  return V;
  }

transmatrix atscreenpos(ld x, ld y, ld size) {
  transmatrix V = Id;

  V[0][2] += (x - vid.xcenter);
  V[1][2] += (y - vid.ycenter);
  V[0][0] = size * 2 * hcrossf / crossf;
  V[1][1] = size * 2 * hcrossf / crossf;
  V[2][2] = stereo::scrdist;

  return V;
  }

}
