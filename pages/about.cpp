#include "handler.hpp"


route("/about", about){
   Server.SSR("public/views/about.html",connection);
   return 200;  
}
