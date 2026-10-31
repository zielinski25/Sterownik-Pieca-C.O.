// Test fixture from frozen 201.
#include "RequestRegistry.h"
#include "Model197.inc"
#include "Raw198.inc"
using upload201::Action;
struct Fixture {
 Model model;Lock lock,binding;Backend backend;history197::Permanent<Owner>os;Owner&o;Raw raw;
 using Pool=upload200::Pool<Owner>;history197::Permanent<Pool>ps;Pool&p;
 using Registry=upload201::Registry<Pool>;history197::Permanent<Registry>rs;Registry&r;
 Fixture():backend(model),o(os.construct(backend,lock)),raw(model,o),p(ps.construct(o,raw)),r(rs.construct(p,binding)){assert(o.enable());}
};
