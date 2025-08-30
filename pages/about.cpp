#include "handler.hpp"
Pnix Server;
Global test;


route("/about", about){
   Server.SSR("public/views/about.html",connection);
   return 200;  
}
