#include "handler.hpp"

route("/admin/project",project){
    Server.SSR("public/admin/project.htpp",connection);
    return 200;
}
route("/admin/project/create",project_create){
    std::string post_data = Server.read_post_data(connection);
    return 200;
}

route("/admin/project/delete",project_delete){

    return 200;
}