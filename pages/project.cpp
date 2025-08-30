#include "handler.hpp"

route("/admin/project",project){
    Server.SSR("public/admin/project.htpp",connection);
    return 200;
}
route("/admin/project/create",project_create){

    return 200;
}

route("/admin/project/delete",project_delete){

    return 200;
}