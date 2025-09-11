#include "../models/project_model.hpp"
#include "handler.hpp"
#include <iostream>
#include <ostream>
#include <string>
using namespace nlohmann;

route("/admin/project", project) {
  Server.SSR("public/admin/project.htpp", connection);
  return 200;
}

route("/admin/project/create", project_create) {

  std::string post_data = Server.Read(connection);
  json data_as_json = json::parse(post_data);
  Model<Project> project_mapper;
  project_mapper.bind("title", &Project::name)
      .bind("description", &Project::desc)
      .bind("link", &Project::link);

  auto project_object = project_mapper.parse_one(data_as_json);
  std::cout << "Nama: " << project_object.name << std::endl; 
  std::cout << "Description: " << project_object.desc << std::endl; 
  std::cout << "Link: " << project_object.link << std::endl;
  return Server.Response(connection, 200, "Ok", R"({"message":"Success""})");
}

route("/admin/project/delete", project_delete) { return 200; }
