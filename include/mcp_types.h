#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

namespace mcp {

// ======== 前向声明 ========

class TextItem;
class ImageItem;
class ResourceItem;

// ======== ContentItem 基类 ========

// MCP 协议标准内容类型
// Ref: https://github.com/modelcontextprotocol/specification/blob/main/schema
enum class ContentType { Text, Image, Resource };

class ContentItem {
 public:
  virtual ~ContentItem() = default;

  virtual ContentType GetType() const = 0;
  virtual nlohmann::json ToJson() const = 0;
  virtual bool FromJson(const nlohmann::json& j) = 0;

  // 工厂：JSON → 子类
  static std::unique_ptr<ContentItem> Decode(const nlohmann::json& j);
};

// ======== TextItem ========

class TextItem : public ContentItem {
 public:
  TextItem() = default;
  explicit TextItem(std::string text) : text_(std::move(text)) {}

  ContentType GetType() const override { return ContentType::Text; }
  nlohmann::json ToJson() const override;
  bool FromJson(const nlohmann::json& j) override;

  const std::string& GetText() const { return text_; }
  void SetText(const std::string& text) { text_ = text; }

 private:
  std::string text_;
};

// ======== ImageItem ========

class ImageItem : public ContentItem {
 public:
  ImageItem() = default;
  ImageItem(std::string data, std::string mime)
      : data_(std::move(data)), mimeType_(std::move(mime)) {}

  ContentType GetType() const override { return ContentType::Image; }
  nlohmann::json ToJson() const override;
  bool FromJson(const nlohmann::json& j) override;

  const std::string& GetData() const { return data_; }
  const std::string& GetMimeType() const { return mimeType_; }

 private:
  std::string data_;       // base64 编码
  std::string mimeType_;   // image/png, image/jpeg ...
};

// ======== ResourceItem ========

class ResourceItem : public ContentItem {
 public:
  ResourceItem() = default;
  ResourceItem(std::string uri, std::string mime, std::string text = "")
      : uri_(std::move(uri)),
        mimeType_(std::move(mime)),
        text_(std::move(text)) {}

  ContentType GetType() const override { return ContentType::Resource; }
  nlohmann::json ToJson() const override;
  bool FromJson(const nlohmann::json& j) override;

  const std::string& GetUri() const { return uri_; }
  const std::string& GetMimeType() const { return mimeType_; }
  const std::string& GetText() const { return text_; }

 private:
  std::string uri_;
  std::string mimeType_;
  std::string text_;
};

// ======== ToolResult ========

// 工具执行结果：包含零到多个 ContentItem + 错误标记
class ToolResult {
 public:
  ToolResult() = default;

  // --- 构建 ---
  void Add(std::unique_ptr<ContentItem> item);
  void AddText(const std::string& text);
  void AddImage(const std::string& base64_data, const std::string& mime_type);
  void AddResource(const std::string& uri, const std::string& mime_type,
                   const std::string& text = "");

  void SetError(bool is_error) { is_error_ = is_error; }
  bool IsError() const { return is_error_; }
  bool IsEmpty() const { return items_.empty(); }

  const std::vector<std::unique_ptr<ContentItem>>& Items() const {
    return items_;
  }
  std::vector<std::unique_ptr<ContentItem>>& Items() { return items_; }

  // --- 序列化 ---
  nlohmann::json ToJson() const;
  bool FromJson(const nlohmann::json& j);

  // 便捷：转 JSON 字符串（含 error 包装）
  nlohmann::json ToResultJson() const;
  bool FromResultJson(const nlohmann::json& j);

 private:
  std::vector<std::unique_ptr<ContentItem>> items_;
  bool is_error_ = false;
};

// ======== 资源读取结果 ========

// 封装资源内容，支持 text（UTF-8）和 blob（base64 二进制）
class ResourceResult {
 public:
  // 文本资源
  static ResourceResult Text(std::string data,
                              std::string mime = "text/plain") {
    ResourceResult r;
    r.content_ = std::move(data);
    r.mime_type_ = std::move(mime);
    r.is_binary_ = false;
    return r;
  }

  // 二进制资源（base64 编码）
  static ResourceResult Blob(std::string base64_data,
                              std::string mime) {
    ResourceResult r;
    r.content_ = std::move(base64_data);
    r.mime_type_ = std::move(mime);
    r.is_binary_ = true;
    return r;
  }

  const std::string& GetContent() const { return content_; }
  const std::string& GetMimeType() const { return mime_type_; }
  bool IsBinary() const { return is_binary_; }

 private:
  ResourceResult() = default;
  std::string content_;
  std::string mime_type_;
  bool is_binary_ = false;
};

// ======== Handler 类型 ========

// 工具 handler：JSON 参数 → ToolResult
using ToolHandler = std::function<ToolResult(const nlohmann::json& args)>;

// 资源 handler：返回文本或二进制资源
using ResourceHandler = std::function<ResourceResult()>;

// 提示 handler：JSON 参数 → messages 数组
using PromptHandler = std::function<nlohmann::json(const nlohmann::json& args)>;

// ======== 注册变更回调 ========

// 变更类型
enum class ChangeType { ToolAdded, ResourceAdded, PromptAdded };

// 回调：McpServer 注册新工具/资源/提示时触发
// type:  变更类型
// name:  工具名 / 资源 URI / 提示名
// def:   对应的定义 JSON（ToolDef::ToJson / ResourceDef::ToJson / PromptDef::ToJson）
using ChangeCallback =
    std::function<void(ChangeType type, const std::string& name,
                       const nlohmann::json& def)>;

// ======== 工具输入参数 Schema ========

// 描述一个 MCP 工具的输入参数格式（JSON Schema object 类型）
//
// 用法：
//   ToolInputSchema schema;
//   schema.AddProperty("paper_id", "string", "论文ID", true);
//   schema.AddProperty("keywords", "array", "关键词列表").SetItems("string");
//   nlohmann::json j = schema.ToJson();
//   // → {"type":"object","properties":{"paper_id":{"type":"string","description":"论文ID"},...},"required":["paper_id"]}
//
class ToolInputSchema {
 public:
  ToolInputSchema() = default;

  // 添加属性（简单类型）
  // type: "string" | "integer" | "number" | "boolean" | "array" | "object"
  ToolInputSchema& AddProperty(const std::string& name,
                                const std::string& type,
                                const std::string& description = "",
                                bool required = false);

  // 添加属性（复杂类型：直接传入 JSON Schema fragment）
  // 用于嵌套 object/array 等高级场景
  // 注意：第二个参数必须是 nlohmann::json 类型，避免与 AddProperty(name, type) 歧义
  ToolInputSchema& AddRawProperty(const std::string& name,
                                   const nlohmann::json& property_schema,
                                   bool required = false);

  // 对最后添加的属性设置 array items 类型（该属性 type 必须为 "array"）
  ToolInputSchema& SetItems(const std::string& item_type);

  // 对最后添加的属性设置 enum 候选值
  ToolInputSchema& SetEnum(std::vector<std::string> values);

  // 对最后添加的属性设置默认值
  ToolInputSchema& SetDefault(const nlohmann::json& value);

  // 序列化
  nlohmann::json ToJson() const;
  static ToolInputSchema FromJson(const nlohmann::json& j);

  // 校验 schema 是否有效，返回错误信息（空串表示有效）
  // 检查项：type 为 object、至少一个属性、required 中的属性必须存在
  std::string Validate() const;

  // 访问器
  const std::string& GetType() const { return type_; }
  const nlohmann::json& GetProperties() const { return properties_; }
  const std::vector<std::string>& GetRequired() const { return required_; }
  bool IsEmpty() const { return properties_.empty(); }

 private:
  std::string type_ = "object";
  nlohmann::json properties_ = nlohmann::json::object();
  std::vector<std::string> required_;
  std::string last_property_;  // 记录最后添加的属性名，供 SetItems/SetEnum 定位
};

// ======== 工具定义（可序列化，不含 handler）========

// 描述一个工具的参数格式，用于 tools/list 对外输出
// handler 由 McpServer 单独存储在 tool_handlers_ 中
class ToolDef {
 public:
  ToolDef() = default;
  ToolDef(std::string name, std::string description, ToolInputSchema inputSchema)
      : name_(std::move(name)),
        description_(std::move(description)),
        inputSchema_(std::move(inputSchema)) {}

  const std::string& GetName() const { return name_; }
  void SetName(const std::string& name) { name_ = name; }

  const std::string& GetDescription() const { return description_; }
  void SetDescription(const std::string& desc) { description_ = desc; }

  const ToolInputSchema& GetInputSchema() const { return inputSchema_; }
  void SetInputSchema(const ToolInputSchema& schema) { inputSchema_ = schema; }

  // 序列化：输出符合 MCP tools/list 的 JSON 对象
  nlohmann::json ToJson() const;
  bool FromJson(const nlohmann::json& j);

 private:
  std::string name_;
  std::string description_;
  ToolInputSchema inputSchema_;
};

// ======== 资源定义（可序列化，不含 handler）========

class ResourceDef {
 public:
  ResourceDef() = default;
  ResourceDef(std::string uri, std::string name, std::string description,
              std::string mimeType)
      : uri_(std::move(uri)),
        name_(std::move(name)),
        description_(std::move(description)),
        mimeType_(std::move(mimeType)) {}

  const std::string& GetUri() const { return uri_; }
  void SetUri(const std::string& uri) { uri_ = uri; }

  const std::string& GetName() const { return name_; }
  void SetName(const std::string& name) { name_ = name; }

  const std::string& GetDescription() const { return description_; }
  void SetDescription(const std::string& desc) { description_ = desc; }

  const std::string& GetMimeType() const { return mimeType_; }
  void SetMimeType(const std::string& mime) { mimeType_ = mime; }

  nlohmann::json ToJson() const;
  bool FromJson(const nlohmann::json& j);

 private:
  std::string uri_;
  std::string name_;
  std::string description_;
  std::string mimeType_;
};

// ======== 提示定义（可序列化，不含 handler）========

class PromptDef {
 public:
  PromptDef() = default;
  PromptDef(std::string name, std::string description, nlohmann::json arguments)
      : name_(std::move(name)),
        description_(std::move(description)),
        arguments_(std::move(arguments)) {}

  const std::string& GetName() const { return name_; }
  void SetName(const std::string& name) { name_ = name; }

  const std::string& GetDescription() const { return description_; }
  void SetDescription(const std::string& desc) { description_ = desc; }

  const nlohmann::json& GetArguments() const { return arguments_; }
  void SetArguments(const nlohmann::json& args) { arguments_ = args; }

  nlohmann::json ToJson() const;
  bool FromJson(const nlohmann::json& j);

 private:
  std::string name_;
  std::string description_;
  nlohmann::json arguments_;
};

// ======== 辅助函数 ========

// 快速创建纯文本结果
ToolResult MakeTextResult(const std::string& text);

// 快速创建错误结果
ToolResult MakeErrorResult(const std::string& message);

}  // namespace mcp
