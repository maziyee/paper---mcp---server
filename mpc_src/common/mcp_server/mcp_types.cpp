#include "mcp_types.h"

namespace mcp {

// ======== TextItem ========

nlohmann::json TextItem::ToJson() const {
  return {{"type", "text"}, {"text", text_}};
}

bool TextItem::FromJson(const nlohmann::json& j) {
  if (j.value("type", "") != "text") return false;
  text_ = j.value("text", "");
  return true;
}

// ======== ImageItem ========

nlohmann::json ImageItem::ToJson() const {
  return {{"type", "image"}, {"data", data_}, {"mimeType", mimeType_}};
}

bool ImageItem::FromJson(const nlohmann::json& j) {
  if (j.value("type", "") != "image") return false;
  data_ = j.value("data", "");
  mimeType_ = j.value("mimeType", "image/png");
  return true;
}

// ======== ResourceItem ========

nlohmann::json ResourceItem::ToJson() const {
  nlohmann::json j;
  j["type"] = "resource";
  j["resource"]["uri"] = uri_;
  j["resource"]["mimeType"] = mimeType_;
  if (!text_.empty()) j["resource"]["text"] = text_;
  return j;
}

bool ResourceItem::FromJson(const nlohmann::json& j) {
  if (j.value("type", "") != "resource") return false;
  uri_ = j.value("resource", nlohmann::json::object()).value("uri", "");
  mimeType_ =
      j.value("resource", nlohmann::json::object()).value("mimeType", "");
  text_ =
      j.value("resource", nlohmann::json::object()).value("text", "");
  return true;
}

// ======== ToolInputSchema ========

ToolInputSchema& ToolInputSchema::AddProperty(const std::string& name,
                                               const std::string& type,
                                               const std::string& description,
                                               bool required) {
  nlohmann::json prop;
  prop["type"] = type;
  if (!description.empty()) prop["description"] = description;
  properties_[name] = std::move(prop);
  if (required) required_.push_back(name);
  last_property_ = name;
  return *this;
}

ToolInputSchema& ToolInputSchema::AddRawProperty(
    const std::string& name,
    const nlohmann::json& property_schema,
    bool required) {
  properties_[name] = property_schema;
  if (required) required_.push_back(name);
  last_property_ = name;
  return *this;
}

ToolInputSchema& ToolInputSchema::SetItems(const std::string& item_type) {
  if (!last_property_.empty() && properties_.contains(last_property_)) {
    properties_[last_property_]["items"]["type"] = item_type;
  }
  return *this;
}

ToolInputSchema& ToolInputSchema::SetEnum(std::vector<std::string> values) {
  if (!last_property_.empty() && properties_.contains(last_property_)) {
    nlohmann::json arr = nlohmann::json::array();
    for (auto& v : values) arr.push_back(std::move(v));
    properties_[last_property_]["enum"] = std::move(arr);
  }
  return *this;
}

ToolInputSchema& ToolInputSchema::SetDefault(const nlohmann::json& value) {
  if (!last_property_.empty() && properties_.contains(last_property_)) {
    properties_[last_property_]["default"] = value;
  }
  return *this;
}

nlohmann::json ToolInputSchema::ToJson() const {
  nlohmann::json j;
  j["type"] = type_;
  j["properties"] = properties_;
  if (!required_.empty()) {
    j["required"] = required_;
  }
  return j;
}

ToolInputSchema ToolInputSchema::FromJson(const nlohmann::json& j) {
  ToolInputSchema schema;
  schema.type_ = j.value("type", "object");
  schema.properties_ = j.value("properties", nlohmann::json::object());
  if (j.contains("required") && j["required"].is_array()) {
    for (const auto& r : j["required"]) {
      if (r.is_string()) schema.required_.push_back(r.get<std::string>());
    }
  }
  return schema;
}

// ======== ToolDef ========

nlohmann::json ToolDef::ToJson() const {
  nlohmann::json j;
  j["name"] = name_;
  j["description"] = description_;
  j["inputSchema"] = inputSchema_.ToJson();
  return j;
}

bool ToolDef::FromJson(const nlohmann::json& j) {
  name_ = j.value("name", "");
  description_ = j.value("description", "");
  if (j.contains("inputSchema")) {
    inputSchema_ =
        ToolInputSchema::FromJson(j["inputSchema"]);
  }
  return !name_.empty();
}

// ======== ResourceDef ========

nlohmann::json ResourceDef::ToJson() const {
  nlohmann::json j;
  j["uri"] = uri_;
  j["name"] = name_;
  j["description"] = description_;
  j["mimeType"] = mimeType_;
  return j;
}

bool ResourceDef::FromJson(const nlohmann::json& j) {
  uri_ = j.value("uri", "");
  name_ = j.value("name", "");
  description_ = j.value("description", "");
  mimeType_ = j.value("mimeType", "");
  return !uri_.empty();
}

// ======== PromptDef ========

nlohmann::json PromptDef::ToJson() const {
  nlohmann::json j;
  j["name"] = name_;
  j["description"] = description_;
  j["arguments"] = arguments_;
  return j;
}

bool PromptDef::FromJson(const nlohmann::json& j) {
  name_ = j.value("name", "");
  description_ = j.value("description", "");
  arguments_ = j.value("arguments", nlohmann::json::array());
  return !name_.empty();
}

// ======== ContentItem 工厂 ========

std::unique_ptr<ContentItem> ContentItem::Decode(const nlohmann::json& j) {
  std::string type = j.value("type", "");
  if (type == "text") {
    auto item = std::make_unique<TextItem>();
    item->FromJson(j);
    return item;
  }
  if (type == "image") {
    auto item = std::make_unique<ImageItem>();
    item->FromJson(j);
    return item;
  }
  if (type == "resource") {
    auto item = std::make_unique<ResourceItem>();
    item->FromJson(j);
    return item;
  }
  return nullptr;
}

// ======== ToolResult ========

void ToolResult::Add(std::unique_ptr<ContentItem> item) {
  items_.push_back(std::move(item));
}

void ToolResult::AddText(const std::string& text) {
  items_.push_back(std::make_unique<TextItem>(text));
}

void ToolResult::AddImage(const std::string& base64_data,
                          const std::string& mime_type) {
  items_.push_back(std::make_unique<ImageItem>(base64_data, mime_type));
}

void ToolResult::AddResource(const std::string& uri,
                             const std::string& mime_type,
                             const std::string& text) {
  items_.push_back(std::make_unique<ResourceItem>(uri, mime_type, text));
}

nlohmann::json ToolResult::ToJson() const {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& item : items_) {
    arr.push_back(item->ToJson());
  }
  return arr;
}

bool ToolResult::FromJson(const nlohmann::json& j) {
  if (!j.is_array()) return false;
  items_.clear();
  for (const auto& elem : j) {
    auto item = ContentItem::Decode(elem);
    if (item) items_.push_back(std::move(item));
  }
  return true;
}

nlohmann::json ToolResult::ToResultJson() const {
  nlohmann::json result;
  result["content"] = ToJson();
  if (is_error_) result["isError"] = true;
  return result;
}

bool ToolResult::FromResultJson(const nlohmann::json& j) {
  is_error_ = j.value("isError", false);
  return FromJson(j.value("content", nlohmann::json::array()));
}

// ======== 辅助函数 ========

ToolResult MakeTextResult(const std::string& text) {
  ToolResult r;
  r.AddText(text);
  return r;
}

ToolResult MakeErrorResult(const std::string& message) {
  ToolResult r;
  r.AddText("[Error] " + message);
  r.SetError(true);
  return r;
}

}  // namespace mcp
