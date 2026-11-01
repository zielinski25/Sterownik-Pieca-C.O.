#include <LittleFS.h>
#include "RuntimeFilesystem.h"
#include <type_traits>
using ProductionShape=history197::Owner<fs::LittleFSFS>;
static_assert(!std::is_destructible<ProductionShape>::value,"no destructor may close quarantined files");
static_assert(!std::is_copy_constructible<ProductionShape>::value,"single owner");
static history197::Permanent<ProductionShape> processLifetime;
ProductionShape&construct(fs::LittleFSFS&backend,history197::Mutex&mutex){return processLifetime.construct(backend,mutex);}
