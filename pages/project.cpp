#include "../models/project_model.hpp"
#include "handler.hpp"
#include <iostream>
#include <ostream>
#include <string>
using namespace nlohmann;
const std::string DATABASE = "data.db";

route("/admin/project", project) {
  Server.SSR("public/admin/project.htpp", connection);
  return 200;
}

route("/admin/project/lists", project_lists) {
  Sqlite3 db;
  if (!Sqlite_Open()) {
    return Server.Response(connection, 500, "Internal Server Error",
                           R"({"error":"Failed to open DB"})");
  }

  try {
    auto result = sqlite.SELECT("*").FROM("project").JSON();

    Sqlite_Close();
    return Server.Response(connection, 200, "Ok", result.dump(4));

  } catch (const std::exception &e) {

    Sqlite_Close();
    return Server.Response(connection, 500, "Error",
                           std::string("{\"error\":\"") + e.what() + "\"}");
  }
}

route("/admin/project/create", project_create) {
  json data_as_json = json::parse(Server.Read(connection));
  Model<Project> project_mapper;
  project_mapper.bind("title", &Project::name)
      .bind("description", &Project::desc)
      .bind("link", &Project::link);

  auto project_object = project_mapper.parse_one(data_as_json);

  if (!Sqlite_Open()) {
    return Server.Response(connection, 500, "Internal Server Error",
                           R"({"error":"Failed to open DB"})");
  }

  sqlite
      .INSERT("project", "(title,desc,link)",
              "(" + Escape(project_object.name) + "," +
                  Escape(project_object.desc) + "," +
                  Escape(project_object.link) + ")")
      .execute();

  Sqlite_Close();
  return Server.Response(connection, 200, "Ok", R"({"message":"Success"})");
}

route("/admin/project/delete", project_delete) { return 200; }
