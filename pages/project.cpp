#include "handler.hpp"

route("/admin/project",project){
    Server.SSR("public/admin/project.htpp",connection);
    return 200;
}

route("/admin/project/create",project_create){
    std::string post_data = Server.Read(connection);
    std::cout << post_data << std::endl;
    
    

    return Server.Response(connection,200,"Ok",R"({"message":"Success""})");
}

route("/admin/project/delete",project_delete){

    return 200;
}
