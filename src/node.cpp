#include "node.h"
#include "stx.h"
#include "imrad.h"
#include "cppgen.h"
#include "binding_input.h"
#include "ui_table_columns.h"
#include "ui_message_box.h"
#include "ui_combo_dlg.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
#include <array>

const color32 SNAP_COLOR[] {
	IM_COL32(0, 0, 255, 255),
	IM_COL32(255, 0, 255, 255),
	IM_COL32(0, 0, 255, 255)
};

void toggle(std::vector<UINode*>& c, UINode* val)
{
	auto it = stx::find(c, val);
	if (it == c.end())
		c.push_back(val);
	else
		c.erase(it);
}

std::string Str(const ImVec4& c)
{
	std::ostringstream os;
	os << "IM_COL32(" << c.x << ", " << c.y << ", " << c.z << ", " << c.w << ")";
	return os.str();
}

/*std::string UIContext::ToVarArgs(const std::string& format)
{
	std::string str = "\"";
	std::string args;
	for (size_t i = 0; i < format.size(); ++i)
	{
		if (format[i] == '{')
		{
			if (i + 1 < format.size() && format[i + 1] == '{')
			{
				str += "{";
				++i;
			}
			else
			{
				auto j = format.find('}', i + 1);
				if (j == std::string::npos)
					break;
				std::string name = format.substr(i + 1, j - i - 1);
				const CppGen::Var* var = codeGen->GetVar(name, "");
				if (!var)
					return "\"bad format\"";
				else if (var->type == "int")
				{
					str += "%d";
					args += ", " + name;
				}
				else if (var->type == "float")
				{
					str += "%f";
					args += ", " + name;
				}
				else if (var->type == "std::string")
				{
					str += "%s";
					args += ", " + name + ".c_str()";
				}
				else
					return "\"bad format\"";;
				i = j;
			}
		}
		else if (format[i] == '}')
		{
			if (i + 1 < format.size() && format[i + 1] == '}')
			{
				str += "}";
				++i;
			}
		}
		else
		{
			str += format[i];
		}
	}
	str += "\"";
	str += args;
	return str;x
}*/

void UIContext::ind_up()
{
	ind += codeGen->INDENT;
}

void UIContext::ind_down()
{
	if (ind.size() >= codeGen->INDENT.size())
		ind.resize(ind.size() - codeGen->INDENT.size());
}

//----------------------------------------------------

TopWindow::TopWindow(UIContext& ctx)
{
	flags.prefix("ImGuiWindowFlags_");
	flags.add$(ImGuiWindowFlags_AlwaysAutoResize);
	flags.add$(ImGuiWindowFlags_AlwaysHorizontalScrollbar);
	flags.add$(ImGuiWindowFlags_AlwaysVerticalScrollbar);
	flags.add$(ImGuiWindowFlags_MenuBar);
	flags.add$(ImGuiWindowFlags_NoCollapse);
	flags.add$(ImGuiWindowFlags_NoDocking);
	flags.add$(ImGuiWindowFlags_NoResize);
	flags.add$(ImGuiWindowFlags_NoTitleBar);
}

void TopWindow::Draw(UIContext& ctx)
{
	ctx.level = 0;
	ctx.groupLevel = 0;
	ctx.parent = this;
	ctx.snapParent = nullptr;
	ctx.modalPopup = modalPopup;
	ctx.table = false;

	std::string cap = title.value();
	if (cap.empty())
		cap = "error";
	cap += "##123"; //don't clash 
	int fl = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;
	fl |= flags;

	if (stylePading)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, *stylePading);
	if (styleSpacing)
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, *styleSpacing);
	
	ImGui::SetNextWindowPos(ctx.wpos); // , ImGuiCond_Always, { 0.5, 0.5 });
	
	if (!(fl & ImGuiWindowFlags_AlwaysAutoResize))
		ImGui::SetNextWindowSize({ size_x, size_y });

	bool tmp;
	ImGui::Begin(cap.c_str(), &tmp, fl);

	if (!ctx.snapMode && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
	{
		if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl))
			; //don't participate in group selection
		else
			ctx.selected = { this };
		ImGui::SetKeyboardFocusHere(); //for DEL hotkey reaction
	}
	if (ctx.snapMode && children.empty() && ImGui::IsWindowHovered())
	{
		ctx.snapParent = this;
		ctx.snapIndex = 0;
		ctx.snapSameLine[0] = false;
		ctx.snapNextColumn[0] = false;
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRect(cached_pos, cached_pos + cached_size, SNAP_COLOR[0], 0, 0, 3);
	}

	ImGui::GetCurrentContext()->NavDisableMouseHover = true;
	for (size_t i = 0; i < children.size(); ++i)
	{
		children[i]->Draw(ctx);
	}
	ImGui::GetCurrentContext()->NavDisableMouseHover = false;
	auto mi = ImGui::GetWindowContentRegionMin();
	auto ma = ImGui::GetWindowContentRegionMax();
	cached_pos = ImGui::GetWindowPos() + mi;
	cached_size = ma - mi;

	/*if (ctx.selected == this)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRect(cached_pos, cached_pos + cached_size, IM_COL32(255, 0, 0, 255));
	}*/

	while (ctx.groupLevel) { //fix missing endGroup
		ImGui::EndGroup();
		--ctx.groupLevel;
	}

	ImGui::End();
	
	if (styleSpacing)
		ImGui::PopStyleVar();
	if (stylePading)
		ImGui::PopStyleVar();
}

void TopWindow::Export(std::ostream& os, UIContext& ctx)
{
	ctx.ind = ctx.codeGen->INDENT;
	ctx.groupLevel = 0;
	ctx.modalPopup = modalPopup;
	ctx.errors.clear();
	
	std::string tit = title.to_arg();
	if (tit.empty())
		ctx.errors.push_back("TopWindow: title can't be empty");

	os << ctx.ind << "/// @begin TopWindow\n";
	
	if (stylePading)
	{
		os << ctx.ind << "ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { "
			<< stylePading->x << ", " << stylePading->y << " });\n";
	}
	if (styleSpacing)
	{
		os << ctx.ind << "ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { "
			<< styleSpacing->x << ", " << styleSpacing->y << " });\n";
	}

	if ((flags & ImGuiWindowFlags_AlwaysAutoResize) == 0)
	{
		os << ctx.ind << "ImGui::SetNextWindowSize({ " << size_x << ", " << size_y << " }"
			<< ", ImGuiCond_Appearing);\n";
	}

	if (modalPopup)
	{
		os << ctx.ind << "if (requestOpen) {\n";
		ctx.ind_up();
		os << ctx.ind << "requestOpen = false;\n";
		os << ctx.ind << "ImGui::OpenPopup(" << tit << ");\n";
		ctx.ind_down();
		os << ctx.ind << "}\n";
		os << ctx.ind << "ImVec2 center = ImGui::GetMainViewport()->GetCenter();\n";
		os << ctx.ind << "ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));\n";
		os << ctx.ind << "bool tmpOpen = true;\n";
		os << ctx.ind << "if (ImGui::BeginPopupModal(" << tit << ", &tmpOpen, " << flags.to_arg() << "))\n";
		os << ctx.ind << "{\n";
		ctx.ind_up();
		os << ctx.ind << "if (requestClose)\n";
		ctx.ind_up();
		os << ctx.ind << "ImGui::CloseCurrentPopup();\n";
		ctx.ind_down();
	}
	else
	{
		os << ctx.ind << "bool tmpOpen = true;\n";
		os << ctx.ind << "ImGui::Begin(" << tit << ", &tmpOpen, " << flags.to_arg() << ");\n";
		os << ctx.ind << "{\n";
		ctx.ind_up();
	}
	os << ctx.ind << "/// @separator\n\n";
		
	for (const auto& ch : children)
		ch->Export(os, ctx);

	if (ctx.groupLevel)
		ctx.errors.push_back("missing EndGroup");

	os << ctx.ind << "/// @separator\n";
	
	if (modalPopup)
	{
		os << ctx.ind << "ImGui::EndPopup();\n";
		ctx.ind_down();
		os << ctx.ind << "}\n";
	}
	else
	{
		os << ctx.ind << "ImGui::End();\n";
		ctx.ind_down();
		os << ctx.ind << "}\n";
	}

	if (styleSpacing)
		os << ctx.ind << "ImGui::PopStyleVar();\n";
	if (stylePading)
		os << ctx.ind << "ImGui::PopStyleVar();\n";

	os << ctx.ind << "/// @end TopWindow\n";
}

void TopWindow::Import(cpp::stmt_iterator& sit, UIContext& ctx)
{
	ctx.errors.clear();
	ctx.importState = 1;
	ctx.userCode = "";

	while (sit != cpp::stmt_iterator())
	{
		if (sit->kind == cpp::Comment && !sit->line.compare(0, 11, "/// @begin "))
		{
			ctx.importState = 1;
			sit.enable_parsing(true);
			auto node = Widget::Create(sit->line.substr(11), ctx);
			if (node) {
				node->Import(++sit, ctx);
				children.push_back(std::move(node));
				ctx.importState = 2;
				ctx.userCode = "";
				sit.enable_parsing(false);
			}
		}
		else if (sit->kind == cpp::Comment && !sit->line.compare(0, 14, "/// @separator"))
		{
			if (ctx.importState == 1) {
				ctx.importState = 2;
				ctx.userCode = "";
				sit.enable_parsing(false);
			}
			else {
				ctx.importState = 1;
				sit.enable_parsing(true);
			}
		}
		else if (ctx.importState == 2)
		{
			if (ctx.userCode != "")
				ctx.userCode += "\n";
			ctx.userCode += sit->line;
		}
		else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::PushStyleVar")
		{
			if (sit->params.size() == 2 && sit->params[0] == "ImGuiStyleVar_WindowPadding")
				stylePading = cpp::parse_size(sit->params[1]);
			else if (sit->params.size() == 2 && sit->params[0] == "ImGuiStyleVar_ItemSpacing")
				styleSpacing = cpp::parse_size(sit->params[1]);
		}
		if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::SetNextWindowSize")
		{
			if (sit->params.size()) {
				auto size = cpp::parse_size(sit->params[0]);
				size_x = size.x;
				size_y = size.y;
			}
		}
		else if ((sit->kind == cpp::IfCallBlock || sit->kind == cpp::CallExpr) &&
			sit->callee == "ImGui::BeginPopupModal")
		{
			modalPopup = true;
			title.set_from_arg(sit->params[0]);

			if (sit->params.size() >= 3)
				flags.set_from_arg(sit->params[2]);
		}
		else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::Begin")
		{
			modalPopup = false;
			title.set_from_arg(sit->params[0]);

			if (sit->params.size() >= 3)
				flags.set_from_arg(sit->params[2]);
		}
		++sit;
	}

	ctx.importState = 0;
}

void TopWindow::TreeUI(UIContext& ctx)
{
	ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	std::string str = ctx.codeGen->GetName();
	bool selected = stx::count(ctx.selected, this);
	if (selected)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
	if (ImGui::TreeNodeEx(str.c_str(), 0))
	{
		if (selected)
			ImGui::PopStyleColor();
		if (ImGui::IsItemClicked())
		{
			if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl))
				; // don't participate in group selection toggle(ctx.selected, this);
			else
				ctx.selected = { this };
		}
		for (const auto& ch : children)
			ch->TreeUI(ctx);

		ImGui::TreePop();
	} 
	else if (selected)
		ImGui::PopStyleColor();
}

std::vector<UINode::Prop>
TopWindow::Properties()
{
	return {
		{ "title", &title },
		{ "top.modalPopup", nullptr },
		{ "top.flags", nullptr },
		{ "size_x", nullptr },
		{ "size_y", nullptr },
		{ "top.style_padding", nullptr },
		{ "top.style_spacing", nullptr },
	};
}

bool TopWindow::PropertyUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Text("title");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputBindable("##title", &title, ctx);
		break;
	case 1:
		ImGui::Text("modalPopup");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##modal", &modalPopup);
		break;
	case 2:
		ImGui::Unindent();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
		if (ImGui::TreeNode("flags")) {
			ImGui::TableNextColumn();
			changed = CheckBoxFlags(&flags);
			ImGui::TreePop();
		}
		ImGui::Spacing();
		ImGui::PopStyleVar();
		ImGui::Indent();
		break;
	case 3:
		ImGui::Text("size_x");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = ImGui::InputFloat("##size_x", &size_x);
		break;
	case 4:
		ImGui::Text("size_y");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = ImGui::InputFloat("##size_y", &size_y);
		break;
	case 5:
	{
		ImGui::Text("style_padding");
		ImGui::TableNextColumn();
		int v[2] = { -1, -1 }; 
		if (stylePading) {
			v[0] = (int)stylePading->x;
			v[1] = (int)stylePading->y;
		}
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputInt2("##style_padding", v))
		{
			changed = true;
			if (v[0] >= 0 || v[1] >= 0)
				stylePading = { (float)v[0], (float)v[1] };
			else
				stylePading = {};
		}
		break;
	}
	case 6:
	{
		ImGui::Text("style_spacing");
		ImGui::TableNextColumn();
		int v[2] = { -1, -1 };
		if (styleSpacing) {
			v[0] = (int)styleSpacing->x;
			v[1] = (int)styleSpacing->y;
		}
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputInt2("##style_spacing", v))
		{
			changed = true;
			if (v[0] >= 0 || v[1] >= 0)
				styleSpacing = { (float)v[0], (float)v[1] };
			else
				styleSpacing = {};
		}
		break;
	}
	default:
		return false;
	}
	return changed;
}

std::vector<UINode::Prop>
TopWindow::Events()
{
	return {};
}

bool TopWindow::EventUI(int i, UIContext& ctx)
{
	return false;
}

//-------------------------------------------------

std::unique_ptr<Widget> 
Widget::Create(const std::string& name, UIContext& ctx)
{
	if (name == "Child")
		return std::make_unique<Child>(ctx);
	else if (name == "Text")
		return std::make_unique<Text>(ctx);
	else if (name == "Selectable")
		return std::make_unique<Selectable>(ctx);
	else if (name == "Button")
		return std::make_unique<Button>(ctx);
	else if (name == "CheckBox")
		return std::make_unique<CheckBox>(ctx);
	else if (name == "RadioButton")
		return std::make_unique<RadioButton>(ctx);
	else if (name == "Input")
		return std::make_unique<Input>(ctx);
	else if (name == "Combo")
		return std::make_unique<Combo>(ctx);
	else if (name == "Table")
		return std::make_unique<Table>(ctx);
	else
		return {};
}

Widget::Widget(UIContext& ctx)
{
}

void Widget::Draw(UIContext& ctx)
{
	if (nextColumn) {
		if (ctx.table)
			ImGui::TableNextColumn();
		else
			ImGui::NextColumn();
	}
	if (sameLine) {
		//ImGui::SameLine();
		//ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine(0, spacing * ImGui::GetStyle().ItemSpacing.x);
	} 
	else {
		//ImGui::Separator();
		for (int i = 0; i < spacing; ++i)
			ImGui::Spacing();
	}
	if (indent)
		ImGui::Indent(indent * ImGui::GetStyle().IndentSpacing / 2);
	if (beginGroup) {
		ImGui::BeginGroup();
		++ctx.groupLevel;
	}
	
	auto lastSel = ctx.selected;
	auto* lastParent = ctx.parent;
	ctx.parent = this;
	++ctx.level;
	cached_pos = ImGui::GetCursorScreenPos();
	auto x1 = ImGui::GetCursorPosX();
	ImGui::BeginDisabled(disabled.has_value() && disabled.value());
	ImGui::PushID(this);
	DoDraw(ctx);
	ImGui::PopID();
	ImGui::EndDisabled();
	cached_size = ImGui::GetItemRectSize();
	//corect size.x for wrapped text
	ImGui::SameLine(0, 0);
	cached_size.x = ImGui::GetCursorPosX() - x1;
	ImGui::NewLine();
	--ctx.level;
	ctx.parent = lastParent;

	if (!ctx.snapMode && ctx.selected == lastSel && ImGui::IsItemClicked())
	{
		if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl))
			toggle(ctx.selected, this);
		else
			ctx.selected = { this };
		ImGui::SetKeyboardFocusHere(); //for DEL hotkey reaction
	}
	if (stx::count(ctx.selected, this))
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRect(cached_pos, cached_pos + cached_size, IM_COL32(255, 0, 0, 255));
	}

	if (ctx.snapMode && !ctx.snapParent && ImGui::IsItemHovered())
	{
		DrawSnap(ctx);
	}
	
	if (endGroup && ctx.groupLevel) {
		ImGui::EndGroup();
		--ctx.groupLevel;
	}
}

void Widget::Export(std::ostream& os, UIContext& ctx)
{
	if (userCode != "")
		os << userCode << "\n";
	
	std::string stype = typeid(*this).name();
	auto i = stype.find(' ');
	if (i != std::string::npos)
		stype.erase(0, i + 1);
	auto it = stx::find_if(stype, [](char c) { return isalpha(c);});
	if (it != stype.end())
		stype.erase(0, it - stype.begin());
	os << ctx.ind << "/// @begin " << stype << "\n";

	if (nextColumn)
	{
		if (ctx.table)
			os << ctx.ind << "ImGui::TableNextColumn();\n";
		else
			os << ctx.ind << "ImGui::NextColumn();\n";
	}
	if (sameLine)
	{
		os << ctx.ind << "ImGui::SameLine(";
		if (spacing)
			os << "0, " << spacing << " * ImGui::GetStyle().ItemSpacing.x";
		os << ");\n";
	}
	else
	{
		for (int i = 0; i < spacing; ++i)
			os << ctx.ind << "ImGui::Spacing();\n";
	}
	if (indent)
	{
		os << ctx.ind << "ImGui::Indent(" << indent << " * ImGui::GetStyle().IndentSpacing / 2);\n";
	}
	if (beginGroup)
	{
		os << ctx.ind << "ImGui::BeginGroup();\n";
		++ctx.groupLevel;
	}
	if (!visible.has_value() || !visible.value())
	{
		os << ctx.ind << "if (" << visible.c_str() << ")\n" << ctx.ind << "{\n"; 
		ctx.ind_up();
	}
	if (!disabled.has_value() || disabled.value())
	{
		os << ctx.ind << "ImGui::BeginDisabled(" << disabled.c_str() << ");\n";
	}

	DoExport(os, ctx);

	if (cursor != ImGuiMouseCursor_Arrow)
	{
		os << ctx.ind << "if (ImGui::IsItemHovered())\n";
		ctx.ind_up();
		os << ctx.ind << "ImGui::SetMouseCursor(" << cursor.to_arg() << ");\n";
		ctx.ind_down();
	}
	if (!tooltip.empty())
	{
		os << ctx.ind << "if (ImGui::IsItemHovered())\n";
		ctx.ind_up();
		os << ctx.ind << "ImGui::SetTooltip(" << tooltip.to_arg() << ");\n";
		ctx.ind_down();
	}
	if (!disabled.has_value() || disabled.value())
	{
		os << ctx.ind << "ImGui::EndDisabled();\n";
	}
	
	if (!onItemHovered.empty())
	{
		os << ctx.ind << "if (ImGui::IsItemHovered())\n";
		ctx.ind_up();
		os << ctx.ind << onItemHovered.c_str() << "();\n";
		ctx.ind_down();
	}
	if (!onItemClicked.empty())
	{
		os << ctx.ind << "if (ImGui::IsItemClicked())\n";
		ctx.ind_up();
		os << ctx.ind << onItemClicked.c_str() << "();\n";
		ctx.ind_down();
	}
	if (!onItemDoubleClicked.empty())
	{
		os << ctx.ind << "if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered())\n";
		ctx.ind_up();
		os << ctx.ind << onItemDoubleClicked.c_str() << "();\n";
		ctx.ind_down();
	}
	if (!onItemFocused.empty())
	{
		os << ctx.ind << "if (ImGui::IsItemFocused())\n";
		ctx.ind_up();
		os << ctx.ind << onItemFocused.c_str() << "();\n";
		ctx.ind_down();
	}
	if (!onItemActivated.empty())
	{
		os << ctx.ind << "if (ImGui::IsItemActivated())\n";
		ctx.ind_up();
		os << ctx.ind << onItemActivated.c_str() << "();\n";
		ctx.ind_down();
	}
	if (!onItemDeactivated.empty())
	{
		os << ctx.ind << "if (ImGui::IsItemDeactivated())\n";
		ctx.ind_up();
		os << ctx.ind << onItemDeactivated.c_str() << "();\n";
		ctx.ind_down();
	}
	if (!onItemDeactivatedAfterEdit.empty())
	{
		os << ctx.ind << "if (ImGui::IsItemDeactivatedAfterEdit())\n";
		ctx.ind_up();
		os << ctx.ind << onItemDeactivatedAfterEdit.c_str() << "();\n";
		ctx.ind_down();
	}

	if (!visible.has_value() || !visible.value())
	{
		ctx.ind_down();
		os << ctx.ind << "}\n";
	}
	if (endGroup && ctx.groupLevel) 
	{
		os << ctx.ind << "ImGui::EndGroup();\n";
		--ctx.groupLevel;
	}

	os << ctx.ind << "/// @end " << stype << "\n\n";
}

void Widget::Import(cpp::stmt_iterator& sit, UIContext& ctx)
{
	ctx.importState = 1;
	userCode = ctx.userCode;

	while (sit != cpp::stmt_iterator())
	{
		if (sit->kind == cpp::Comment && !sit->line.compare(0, 11, "/// @begin "))
		{
			ctx.importState = 1;
			std::string name = sit->line.substr(11);
			auto w = Widget::Create(name, ctx);
			if (!w) {
				//uknown control 
				//create a placeholder not to break parsing and layout
				ctx.errors.push_back("Encountered an unknown control '" + name + "'");
				auto txt = std::make_unique<Text>(ctx);
				txt->text = "???";
				w = std::move(txt);
			}
			w->Import(++sit, ctx);
			children.push_back(std::move(w));
			ctx.importState = 2;
			ctx.userCode = "";
		}
		else if (sit->kind == cpp::Comment && !sit->line.compare(0, 9, "/// @end "))
		{
			break;
		}
		else if (sit->kind == cpp::Comment && !sit->line.compare(0, 14, "/// @separator"))
		{
			if (ctx.importState == 1) {
				ctx.importState = 2;
				ctx.userCode = "";
			}
			else
				ctx.importState = 1;
		}
		else if (ctx.importState == 2)
		{
			if (ctx.userCode != "")
				ctx.userCode += "\n";
			ctx.userCode += sit->line;
		}
		else if (sit->kind == cpp::IfBlock) //todo: weak condition
		{
			visible.set_from_arg(sit->cond);
		}
		else if (sit->kind == cpp::CallExpr && 
			(sit->callee == "ImGui::NextColumn" || sit->callee == "ImGui::TableNextColumn"))
		{
			nextColumn = true;
		}
		else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::SameLine")
		{
			sameLine = true;
			if (sit->params.size() == 2)
				spacing.set_from_arg(sit->params[1]);
		}
		else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::Spacing")
		{
			++spacing;
		}
		else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::Indent")
		{
			if (sit->params.size())
				indent.set_from_arg(sit->params[0]);
		}
		else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::BeginGroup")
		{
			beginGroup = true;
		}
		else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::EndGroup")
		{
			endGroup = true;
		}
		else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::BeginDisabled")
		{
			if (sit->params.size())
				disabled.set_from_arg(sit->params[0]);
		}
		/*else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::SetToolTip")
		{
			if (sit->params.size())
				tooltip.set_from_arg(sit->params[0]);
		}*/
		else if (sit->kind == cpp::IfCallThenCall && sit->cond == "ImGui::IsItemHovered")
		{
			if (sit->callee == "ImGui::SetTooltip")
				tooltip.set_from_arg(sit->params2[0]);
			else if (sit->callee == "ImGui::SetMouseCursor")
				cursor.set_from_arg(sit->params2[0]);
			else
				onItemHovered.set_from_arg(sit->callee);
		}
		else if (sit->kind == cpp::IfCallThenCall && sit->cond == "ImGui::IsItemClicked")
		{
			onItemClicked.set_from_arg(sit->callee);
		}
		else if (sit->kind == cpp::IfCallThenCall && sit->cond == "ImGui::IsMouseDoubleClicked")
		{
			onItemDoubleClicked.set_from_arg(sit->callee);
		}
		else if (sit->kind == cpp::IfCallThenCall && sit->cond == "ImGui::IsItemFocused")
		{
			onItemFocused.set_from_arg(sit->callee);
		}
		else if (sit->kind == cpp::IfCallThenCall && sit->cond == "ImGui::IsItemActivated")
		{
			onItemActivated.set_from_arg(sit->callee);
		}
		else if (sit->kind == cpp::IfCallThenCall && sit->cond == "ImGui::IsItemDeactivated")
		{
			onItemDeactivated.set_from_arg(sit->callee);
		}
		else if (sit->kind == cpp::IfCallThenCall && sit->cond == "ImGui::IsItemDeactivatedAfterEdit")
		{
			onItemDeactivatedAfterEdit.set_from_arg(sit->callee);
		}
		else
		{
			DoImport(sit, ctx);
		}
		++sit;
	}
}

void Widget::DrawSnap(UIContext& ctx)
{
	ImGuiDir snapDir;
	ImVec2 d1 = ImGui::GetMousePos() - cached_pos;
	ImVec2 d2 = cached_pos + cached_size - ImGui::GetMousePos();
	float mind = std::min({ d1.x, d1.y, d2.x, d2.y });
	if (mind > 7)
		snapDir = ImGuiDir_None;
	else if (d1.x == mind)
		snapDir = ImGuiDir_Left;
	else if (d2.x == mind)
		snapDir = ImGuiDir_Right;
	else if (d1.y == mind)
		snapDir = ImGuiDir_Up;
	else if (d2.y == mind)
		snapDir = ImGuiDir_Down;

	const auto& pchildren = ctx.parent->children;
	size_t i = stx::find_if(pchildren, [&](const auto& ch) {
		return ch.get() == this;
		}) - pchildren.begin();
	if (i == pchildren.size())
		return;
	ctx.snapSameLine[0] = false;
	ctx.snapSameLine[1] = false;
	ctx.snapNextColumn[0] = false;
	ctx.snapNextColumn[1] = false;
	ctx.snapBeginGroup[0] = false;
	ctx.snapBeginGroup[1] = false;
	ImVec2 p;
	float w = 0, h = 0;
	switch (snapDir)
	{
	case ImGuiDir_None:
	{
		if (IsContainer() && children.empty())
		{
			ctx.snapParent = this;
			ctx.snapIndex = 0;
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRect(cached_pos, cached_pos + cached_size, SNAP_COLOR[(ctx.level + 1) % std::size(SNAP_COLOR)], 0, 0, 3);
			return;
		}
		break;
	}
	case ImGuiDir_Left:
	{
		p = cached_pos;
		h = cached_size.y;
		if (i && pchildren[i]->sameLine)
		{
			const auto& ch = pchildren[i - 1];
			auto p2 = ch->cached_pos + ImVec2(ch->cached_size.x, 0);
			auto h2 = ch->cached_size.y;
			ImVec2 q{ (p.x + p2.x) / 2, std::min(p.y, p2.y) };
			h = std::max(p.y + h, p2.y + h2) - q.y;
			p = q;
		}
		ctx.snapParent = ctx.parent;
		ctx.snapIndex = i;
		ctx.snapSameLine[0] = pchildren[i]->sameLine;
		ctx.snapSameLine[1] = true;
		ctx.snapNextColumn[0] = pchildren[i]->nextColumn;
		ctx.snapNextColumn[1] = false;
		ctx.snapBeginGroup[1] = pchildren[i]->beginGroup;
		break;
	}
	case ImGuiDir_Right:
	{
		p = cached_pos + ImVec2(cached_size.x, 0);
		h = cached_size.y;
		bool last = i + 1 == pchildren.size();
		bool lastCol = true;
		bool nextCol = false;
		auto *pch = i + 1 < pchildren.size() ? pchildren[i + 1].get() : nullptr;
		if (pch && pchildren[i + 1]->sameLine)
		{
			auto p2 = pch->cached_pos;
			auto h2 = pch->cached_size.y;
			ImVec2 q{ (p.x + p2.x) / 2, std::min(p.y, p2.y) };
			h = std::max(p.y + h, p2.y + h2) - q.y;
			p = q;
		}
		ctx.snapParent = ctx.parent;
		ctx.snapIndex = i + 1;
		ctx.snapSameLine[0] = true;
		ctx.snapSameLine[1] = pch ? (bool)pch->sameLine : false;
		ctx.snapNextColumn[0] = false;
		ctx.snapNextColumn[1] = pch ? (bool)pch->nextColumn : false;
		ctx.snapBeginGroup[1] = pch ? (bool)pch->beginGroup : false;
		break;
	}
	case ImGuiDir_Up:
	case ImGuiDir_Down:
	{
		bool down = snapDir == ImGuiDir_Down;
		p = cached_pos;
		if (down)
			p.y += cached_size.y;
		float x2 = p.x + cached_size.x;
		size_t i1 = i, i2 = i;
		for (int j = (int)i - 1; j >= 0; --j)
		{
			if (!pchildren[j + 1]->sameLine)
				break;
			if (pchildren[j + 1]->beginGroup)
				break;
			assert(!pchildren[j + 1]->nextColumn);
			i1 = j;
			const auto& ch = pchildren[j];
			p.x = ch->cached_pos.x;
			if (down)
				p.y = std::max(p.y, ch->cached_pos.y + ch->cached_size.y);
			else
				p.y = std::min(p.y, ch->cached_pos.y);
		}
		for (int j = (int)i + 1; j < (int)pchildren.size(); ++j)
		{
			if (!pchildren[j]->sameLine)
				break;
			assert(!pchildren[j]->nextColumn);
			i2 = j;
			const auto& ch = pchildren[j];
			x2 = ch->cached_pos.x + ch->cached_size.x;
			if (down)
				p.y = std::max(p.y, ch->cached_pos.y + ch->cached_size.y);
			else
				p.y = std::min(p.y, ch->cached_pos.y);
		}
		w = x2 - p.x;
		ctx.snapSameLine[0] = pchildren[i1]->sameLine && pchildren[i1]->beginGroup;
		ctx.snapSameLine[1] = false;
		ctx.snapParent = ctx.parent;
		if (down)
		{
			ctx.snapNextColumn[0] = false;
			ctx.snapNextColumn[1] = i2 + 1 < pchildren.size() ? (bool)pchildren[i2 + 1]->nextColumn : false;
			ctx.snapIndex = i2 + 1;
		}
		else
		{
			ctx.snapNextColumn[0] = pchildren[i1]->nextColumn;
			ctx.snapNextColumn[1] = false;
			ctx.snapBeginGroup[0] = pchildren[i1]->beginGroup;
			ctx.snapBeginGroup[1] = false;
			ctx.snapIndex = i1;
		}
		break;
	}
	default:
		return;
	}

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddLine(p, p + ImVec2(w, h), SNAP_COLOR[ctx.level % std::size(SNAP_COLOR)], 3);
}

std::vector<UINode::Prop>
Widget::Properties()
{
	return {
		{ "visible", &visible },
		{ "cursor", &cursor },
		{ "tooltip", &tooltip },
		{ "disabled", &disabled },
		{ "indent", &indent },
		{ "spacing", &spacing },
		{ "sameLine", &sameLine },
		{ "beginGroup", &beginGroup },
		{ "endGroup", &endGroup },
		{ "nextColumn", &nextColumn },
	};
}

bool Widget::PropertyUI(int i, UIContext& ctx)
{
	int sat = (i & 1) ? 202 : 164;
	if (i <= 3)
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(sat, 255, sat, 255));
	else
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, sat, 255));
	
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Text("visible");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##visible", &visible, true, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("visible", &visible, ctx);
		break;
	case 1:
		ImGui::Text("cursor");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = ImGui::Combo("##cursor", cursor.access(), "Arrow\0TextInput\0ResizeAll\0ResizeNS\0ResizeEW\0ResizeNESW\0ResizeNWSE\0Hand\0NotAllowed\0\0");
		break;
	case 2:
		ImGui::Text("tooltip");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = ImGui::InputText("##tooltip", tooltip.access());
		break;
	case 3:
		ImGui::Text("disabled");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##disabled", &disabled, false, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("disabled", &disabled, ctx);
		break;
	case 4:
		ImGui::BeginDisabled(sameLine);
		ImGui::Text("indent");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = ImGui::InputInt("##indent", indent.access());
		if (ImGui::IsItemDeactivatedAfterEdit() && indent < 0)
		{
			changed = true;
			indent = 0;
		}
		ImGui::EndDisabled();
		break;
	case 5:
		ImGui::Text("spacing");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = ImGui::InputInt("##spacing", spacing.access());
		if (ImGui::IsItemDeactivatedAfterEdit() && spacing < 0)
		{
			changed = true;
			spacing = 0;
		}
		break;
	case 6:
		ImGui::BeginDisabled(nextColumn);
		ImGui::Text("sameLine");
		ImGui::TableNextColumn();
		if (ImGui::Checkbox("##sameLine", sameLine.access())) {
			changed = true;
			if (sameLine)
				indent = 0;
		}
		ImGui::EndDisabled();
		break;
	case 7:
		ImGui::Text("beginGroup");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##beginGroup", beginGroup.access());
		break;
	case 8:
		ImGui::Text("endGroup");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##endGroup", endGroup.access());
		break;
	case 9:
		ImGui::Text("nextColumn");
		ImGui::TableNextColumn();
		if (ImGui::Checkbox("##nextColumn", nextColumn.access())) {
			changed = true;
			if (nextColumn)
				sameLine = false;
		}
		break;
	default:
		return false;
	}
	return changed;
}

std::vector<UINode::Prop>
Widget::Events()
{
	return {
		{ "IsItemHovered", &onItemHovered },
		{ "IsItemClicked", &onItemClicked },
		{ "IsItemDoubleClicked", &onItemDoubleClicked },
		{ "IsItemFocused", &onItemFocused },
		{ "IsItemActivated", &onItemActivated },
		{ "IsItemDeactivated", &onItemDeactivated },
		{ "IsItemDeactivatedAfterEdit", &onItemDeactivatedAfterEdit },
	};
}

bool Widget::EventUI(int i, UIContext& ctx)
{
	bool changed = false;
	int sat = (i & 1) ? 202 : 164;
	ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, sat, 255));
	switch (i)
	{
	case 0:
		ImGui::Text("IsItemHovered");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##IsItemHovered", &onItemHovered, ctx);
		break;
	case 1:
		ImGui::Text("IsItemClicked");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##itemclicked", &onItemClicked, ctx);
		break;
	case 2:
		ImGui::Text("IsItemDoubleClicked");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##itemdblclicked", &onItemDoubleClicked, ctx);
		break;
	case 3:
		ImGui::Text("IsItemFocused");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##IsItemFocused", &onItemFocused, ctx);
		break;
	case 4:
		ImGui::Text("IsItemActivated");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##IsItemActivated", &onItemActivated, ctx);
		break;
	case 5:
		ImGui::Text("IsItemDeactivated");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##IsItemDeactivated", &onItemDeactivated, ctx);
		break;
	case 6:
		ImGui::Text("IsItemDeactivatedAfterEdit");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##IsItemDeactivatedAfterEdit", &onItemDeactivatedAfterEdit, ctx);
		break;
	default:
		return false;
	}
	return changed;
}

void Widget::TreeUI(UIContext& ctx)
{
	ImGui::SetNextItemOpen(true, ImGuiCond_Always);
	std::string str = typeid(*this).name();
	auto i = str.find(' ');
	if (i != std::string::npos)
		str.erase(0, i + 1);
	auto it = stx::find_if(str, [](char c) { return isalpha(c);});
	if (it != str.end())
		str.erase(0, it - str.begin());
	//str = ICON_FA_CLOSED_CAPTIONING " " + str;
	std::string suff;
	if (sameLine)
		suff += "L";
	if (beginGroup)
		suff += "B";
	if (endGroup)
		suff += "E";
	if (nextColumn)
		suff += "C";
	//if (ctx.selected == this)
	bool selected = stx::count(ctx.selected, this);
	if (selected)
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
	if (ImGui::TreeNodeEx(str.c_str(), ImGuiTreeNodeFlags_Leaf)) // | (selected ? ImGuiTreeNodeFlags_Bullet : 0)))
	{
		if (selected)
			ImGui::PopStyleColor();

		if (ImGui::IsItemClicked())
		{
			if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl))
				toggle(ctx.selected, this);
			else
				ctx.selected = { this };
		}
		ImGui::SameLine();
		ImGui::TextDisabled(suff.c_str());
		for (const auto& ch : children)
			ch->TreeUI(ctx);
		ImGui::TreePop();
	}
	else if (selected) {
		ImGui::PopStyleColor();
	}
}

//----------------------------------------------------

void Separator::DoDraw(UIContext& ctx)
{
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
}

void Separator::DoExport(std::ostream& os, UIContext& ctx)
{
}

void Separator::DoImport(const cpp::stmt_iterator& sit, UIContext& ctx)
{
}

bool Separator::PropertyUI(int i, UIContext& ctx)
{
	return Widget::PropertyUI(i, ctx);
}

//----------------------------------------------------

Text::Text(UIContext& ctx)
	: Widget(ctx)
{
}

void Text::DoDraw(UIContext& ctx)
{
	//ImGui::GetIO().Fonts->AddFontFromFileTTF("font.ttf", size_pixels);
	//PushFont
	//PopFont

	std::optional<color32> clr;
	if (grayed)
		clr = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	else if (color.has_value())
		clr = color.value();
	if (clr)
		ImGui::PushStyleColor(ImGuiCol_Text, *clr);
	
	auto align = alignment == "Center" ? ImRad::Align_Center :
		alignment == "Right" ? ImRad::Align_Right :
		ImRad::Align_Left;
	if (size_x.has_value() && size_x.value())
		ImGui::SetNextItemWidth(size_x.value());
	if (alignToFrame)
		ImGui::AlignTextToFramePadding();
	
	ImRad::AlignedText(align, text.c_str());

	if (clr)
		ImGui::PopStyleColor();
}

void Text::DoExport(std::ostream& os, UIContext& ctx)
{
	if (!size_x.has_value() || size_x.value())
		os << ctx.ind << "ImGui::SetNextItemWidth(" << size_x.to_arg() << ");\n";

	std::string clr;
	if (grayed)
		clr = "ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]";
	else if (!color.empty())
		clr = color.to_arg();

	if (clr != "")
		os << ctx.ind << "ImGui::PushStyleColor(ImGuiCol_Text, " << clr << ");\n";

	if (alignToFrame)
		os << ctx.ind << "ImGui::AlignTextToFramePadding();\n";

	os << ctx.ind << "ImRad::AlignedText(";
	//alignment.c_str so no apostrophes
	os << "ImRad::Align_" << alignment.c_str() << ", " 
		<< text.to_arg() << ");\n";

	if (clr != "")
		os << ctx.ind << "ImGui::PopStyleColor();\n";
}

void Text::DoImport(const cpp::stmt_iterator& sit, UIContext& ctx)
{
	/*if (sit->kind == cpp::CallExpr && 
		(sit->callee == "ImGui::Text" || sit->callee == "ImGui::TextWrapped"))
	{
		//wrapped = sit->callee == "ImGui::TextWrapped";
		*text.access() = cpp::parse_var_args(sit->params);
	}*/
	if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::SetNextItemWidth")
	{
		if (sit->params.size() >= 1)
			size_x.set_from_arg(sit->params[0]);
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::AlignTextToFramePadding")
	{
		alignToFrame = true;
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImRad::AlignedText")
	{
		if (sit->params.size() >= 1 && !sit->params[0].compare(0, 13, "ImRad::Align_")) {
			std::string hack = "\"";
			hack += sit->params[0].substr(13);
			hack += "\"";
			alignment.set_from_arg(hack);
		}
		if (sit->params.size() >= 2)
			text = cpp::parse_str_arg(sit->params[1]);
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::PushStyleColor")
	{
		if (sit->params.size() >= 2 && sit->params[0] == "ImGuiCol_Text")
		{
			if (sit->params[1] == "ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]")
				grayed = true;
			else
				color.set_from_arg(sit->params[1]);
		}
	}
}

std::vector<UINode::Prop>
Text::Properties()
{
	auto props = Widget::Properties();
	props.insert(props.begin(), {
		{ "text", &text },
		{ "text.grayed", &grayed },
		{ "color", &color },
		{ "text.alignToFramePadding", &alignToFrame },
		{ "text.alignment", &alignment },
		{ "size_x", &size_x },
	});
	return props;
}

bool Text::PropertyUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Text("text");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##text", &text, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("text", &text, ctx);
		break;
	case 1:
		ImGui::Text("grayed");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##grayed", grayed.access());
		break;
	case 2:
	{
		ImGui::BeginDisabled(grayed);
		ImGui::Text("color");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##color", &color, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("color", &color, ctx);
		ImGui::EndDisabled();
		break;
	}
	case 3:
		ImGui::Text("alignToFramePadding");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##alignToFrame", alignToFrame.access());
		break;
	case 4:
	{
		ImGui::BeginDisabled(size_x.has_value() && !size_x.value());
		ImGui::Text("alignment");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		static const char* Alignments[]{ "Left", "Center", "Right" };
		int idx = int(stx::find(Alignments, alignment.to_arg()) - Alignments);
		//std::string vals = stx::join(Alignments, std::string_view("\0", 1));
		//vals += std::string_view("\0\0", 2);
		changed = ImGui::Combo("##wrapped", &idx, Alignments, (int)std::size(Alignments));
		if (changed)
			alignment = Alignments[idx];
		ImGui::EndDisabled();
		break;
	}
	case 5:
		ImGui::Text("size_x");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_x", &size_x, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_x", &size_x, ctx);
		break;
	default:
		return Widget::PropertyUI(i - 6, ctx);
	}
	return changed;
}

//----------------------------------------------------

Selectable::Selectable(UIContext& ctx)
	: Widget(ctx)
{
	flags.prefix("ImGuiSelectableFlags_");
	flags.add$(ImGuiSelectableFlags_DontClosePopups);
	flags.add$(ImGuiSelectableFlags_SpanAllColumns);
}

void Selectable::DoDraw(UIContext& ctx)
{
	std::optional<color32> clr;
	/*if (grayed)
		clr = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	else*/ if (color.has_value())
		clr = color.value();
	if (clr)
		ImGui::PushStyleColor(ImGuiCol_Text, *clr);

	ImVec2 size(0, 0);
	if (size_x.has_value())
		size.x = size_x.value();
	if (size_y.has_value())
		size.y = size_y.value();
	ImGui::Selectable(text.c_str(), false, flags, size);

	if (clr)
		ImGui::PopStyleColor();
}

void Selectable::DoExport(std::ostream& os, UIContext& ctx)
{
	std::string clr;
	/*if (grayed)
		clr = "ImGui::GetStyle().Colors[ImGuiCol_TextDisabled])";
	else*/ if (!color.empty())
		clr = color.to_arg();

	if (clr != "")
		os << ctx.ind << "ImGui::PushStyleColor(ImGuiCol_Header, " << clr << ");\n";

	os << ctx.ind;
	if (!onChange.empty())
		os << "if (";
	
	os << "ImGui::Selectable(" << text.to_arg() << ", false, " 
		<< flags.to_arg() << ", "
		<< "{ " << size_x.to_arg() << ", " << size_y.to_arg() << " })";
	
	if (!onChange.empty()) {
		os << ")\n";
		ctx.ind_up();
		os << ctx.ind << onChange.to_arg() << "();\n";
		ctx.ind_down();
	}
	else {
		os << ";\n";
	}

	if (clr != "")
		os << ctx.ind << "ImGui::PopStyleColor();\n";
}

void Selectable::DoImport(const cpp::stmt_iterator& sit, UIContext& ctx)
{
	if ((sit->kind == cpp::CallExpr && sit->callee == "ImGui::Selectable") ||
		(sit->kind == cpp::IfCallThenCall && sit->cond == "ImGui::Selectable"))
	{
		if (sit->params.size() >= 1)
			text.set_from_arg(sit->params[0]);
		if (sit->params.size() >= 3)
			flags.set_from_arg(sit->params[2]);
		if (sit->params.size() >= 4) {
			auto sz = cpp::break_size(sit->params[3]);
			size_x.set_from_arg(sz[0]);
			size_y.set_from_arg(sz[1]);
		}

		if (sit->kind == cpp::IfCallThenCall)
			onChange.set_from_arg(sit->callee);
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::PushStyleColor")
	{
		if (sit->params.size() >= 2 && sit->params[0] == "ImGuiCol_Header")
		{
			color.set_from_arg(sit->params[1]);
		}
	}
}

std::vector<UINode::Prop>
Selectable::Properties()
{
	auto props = Widget::Properties();
	props.insert(props.begin(), {
		{ "text", &text },
		{ "color", &color },
		{ "selectable.flags", &flags },
		{ "size_x", &size_x },
		{ "size_y", &size_y }
		});
	return props;
}

bool Selectable::PropertyUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Selectable("text");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##text", &text, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("text", &text, ctx);
		break;
	case 1:
		ImGui::Selectable("color");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##color", &color, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("color", &color, ctx);
		break;
	case 2:
		ImGui::Unindent();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
		if (ImGui::TreeNode("flags")) {
			ImGui::TableNextColumn();
			changed = CheckBoxFlags(&flags);
			ImGui::TreePop();
		}
		ImGui::Spacing();
		ImGui::PopStyleVar();
		ImGui::Indent();
		break;
	case 3:
		ImGui::Selectable("size_x");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_x", &size_x, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_x", &size_x, ctx);
		break;
	case 4:
		ImGui::Selectable("size_y");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_y", &size_y, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_y", &size_y, ctx);
		break;
	default:
		return Widget::PropertyUI(i - 5, ctx);
	}
	return changed;
}

std::vector<UINode::Prop>
Selectable ::Events()
{
	auto props = Widget::Events();
	props.insert(props.begin(), {
		{ "onChange", &onChange },
		});
	return props;
}

bool Selectable::EventUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Text("onChange");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##onChange", &onChange, ctx);
		break;
	default:
		return Widget::EventUI(i - 1, ctx);
	}
	return changed;
}

//----------------------------------------------------

Button::Button(UIContext& ctx)
	: Widget(ctx)
{
	assert(ImRad::ModalResult_Count == 6);
	modalResult.add(" ", ImRad::None);
	modalResult.add$(ImRad::Ok);
	modalResult.add$(ImRad::Cancel);
	modalResult.add$(ImRad::Yes);
	modalResult.add$(ImRad::No);
	modalResult.add$(ImRad::All);
	
	arrowDir.add$(ImGuiDir_None);
	arrowDir.add$(ImGuiDir_Left);
	arrowDir.add$(ImGuiDir_Right);
	arrowDir.add$(ImGuiDir_Up);
	arrowDir.add$(ImGuiDir_Down);
}

void Button::DoDraw(UIContext& ctx)
{
	if (color.has_value())
		ImGui::PushStyleColor(ImGuiCol_Button, color.value());
	
	if (arrowDir != ImGuiDir_None)
		ImGui::ArrowButton("", arrowDir);
	else if (small)
		ImGui::SmallButton(text.c_str());
	else
	{
		ImVec2 size{ 0, 0 };
		if (size_x.has_value())
			size.x = size_x.value();
		if (size_y.has_value())
			size.y = size_y.value();
		ImGui::Button(text.c_str(), size);

		//if (ctx.modalPopup && text.value() == "OK")
			//ImGui::SetItemDefaultFocus();
	}
	
	if (color.has_value())
		ImGui::PopStyleColor();
}

void Button::DoExport(std::ostream& os, UIContext& ctx)
{
	if (!color.empty())
		os << ctx.ind << "ImGui::PushStyleColor(ImGuiCol_Button, " << color.to_arg() << ");\n";

	if (arrowDir != ImGuiDir_None)
	{
		os << ctx.ind << "ImGui::ArrowButton(\"###arrow\", " << arrowDir.to_arg() << ");\n";
	}
	else if (small)
	{
		os << ctx.ind << "ImGui::SmallButton(" << text.to_arg() << ");\n";
	}
	else
	{
		bool closePopup = ctx.modalPopup && modalResult != ImRad::None;

		os << ctx.ind;
		if (!onChange.empty() || closePopup)
			os << "if (";

		os << "ImGui::Button(" << text.to_arg() << ", "
			<< "{ " << size_x.to_arg() << ", " << size_y.to_arg() << " }"
			<< ")";

		if (!onChange.empty() || closePopup)
		{
			if (modalResult == ImRad::Cancel) {
				os << " ||\n";
				ctx.ind_up();
				os << ctx.ind << "ImGui::IsKeyPressed(ImGuiKey_Escape)";
				ctx.ind_down();
			}
			os << ")\n" << ctx.ind << "{\n";
			ctx.ind_up();
			
			if (!onChange.empty())
				os << ctx.ind << onChange.to_arg() << "();\n";
			if (closePopup) {
				os << ctx.ind << "ClosePopup();\n";
				if (modalResult != ImRad::Cancel)
					//no if => easier parsing
					os << ctx.ind << "callback(" << modalResult.to_arg() << ");\n";
			}

			ctx.ind_down();			
			os << ctx.ind << "}\n";
		}
		else
		{
			os << ";\n";
		}
	}

	if (!color.empty())
		os << ctx.ind << "ImGui::PopStyleColor();\n";
}

void Button::DoImport(const cpp::stmt_iterator& sit, UIContext& ctx)
{
	if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::PushStyleColor")
	{
		if (sit->params.size() >= 2 && sit->params[0] == "ImGuiCol_Button")
			color.set_from_arg(sit->params[1]);
	}
	else if ((sit->kind == cpp::CallExpr || sit->kind == cpp::IfCallBlock) && 
		sit->callee == "ImGui::Button")
	{
		ctx.importLevel = sit->level;
		text.set_from_arg(sit->params[0]);
		
		if (sit->params.size() >= 2) {
			auto size = cpp::break_size(sit->params[1]);
			size_x.set_from_arg(size[0]);
			size_y.set_from_arg(size[1]);
		}
	}
	else if ((sit->kind == cpp::CallExpr || sit->kind == cpp::IfCallBlock) &&
		sit->callee == "ImGui::SmallButton")
	{
		ctx.importLevel = sit->level;
		text.set_from_arg(sit->params[0]);
	}
	if ((sit->kind == cpp::CallExpr || sit->kind == cpp::IfCallBlock) &&
		sit->callee == "ImGui::ArrowButton")
	{
		ctx.importLevel = sit->level;
		if (sit->params.size() >= 2)
			arrowDir.set_from_arg(sit->params[1]);
	}
	else if (sit->kind == cpp::CallExpr && sit->level == ctx.importLevel + 1) 
	{
		if (sit->callee == "ClosePopup") {
			if (modalResult == ImRad::None)
				modalResult = ImRad::Cancel;
		}
		else if (sit->callee == "callback" && sit->params.size())
			modalResult.set_from_arg(sit->params[0]);
		else
			onChange.set_from_arg(sit->callee);
	}
}

std::vector<UINode::Prop>
Button::Properties()
{
	auto props = Widget::Properties();
	props.insert(props.begin(), {
		{ "text", &text },
		{ "button.arrowDir", &arrowDir },
		{ "button.modalResult", &modalResult },
		{ "color", &color },
		{ "button.small", &small },
		{ "size_x", &size_x },
		{ "size_y", &size_y },
		});
	return props;
}

bool Button::PropertyUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::BeginDisabled(arrowDir != ImGuiDir_None);
		ImGui::Text("text");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##text", &text, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("text", &text, ctx);
		ImGui::EndDisabled();
		break;
	case 1:
	{
		ImGui::Text("arrowDir");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		if (ImGui::BeginCombo("##arrowDir", arrowDir.get_id().c_str()))
		{
			changed = true;
			for (const auto& item : arrowDir.get_ids())
			{
				if (ImGui::Selectable(item.first.c_str(), arrowDir == item.second))
					arrowDir = item.second;
			}
			ImGui::EndCombo();
		}
		break;
	}
	case 2:
	{
		ImGui::BeginDisabled(!ctx.modalPopup);
		ImGui::Text("modalResult");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		if (ImGui::BeginCombo("##modalResult", modalResult.get_id().c_str()))
		{
			changed = true;
			for (const auto& item : modalResult.get_ids())
			{
				if (ImGui::Selectable(item.first.c_str(), modalResult == item.second))
					modalResult = item.second;
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
		break;
	}
	case 3:
		ImGui::Text("color");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##color", &color, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("color", &color, ctx);
		break;
	case 4:
		ImGui::Text("small");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##small", small.access());
		break;
	case 5:
		ImGui::Text("size_x");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_x", &size_x, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_x", &size_x, ctx);
		break;
	case 6:
		ImGui::Text("size_y");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_y", &size_y, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_y", &size_y, ctx);
		break;
	default:
		return Widget::PropertyUI(i - 7, ctx);
	}
	return changed;
}

std::vector<UINode::Prop>
Button::Events()
{
	auto props = Widget::Events();
	props.insert(props.begin(), {
		{ "onChange", &onChange },
		});
	return props;
}

bool Button::EventUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Text("onChange");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##onChange", &onChange, ctx);
		break;
	default:
		return Widget::EventUI(i - 1, ctx);
	}
	return changed;
}

//-----------------------------------------------------

CheckBox::CheckBox(UIContext& ctx)
	: Widget(ctx)
{
	if (!ctx.importState)
		field_name.set_from_arg(ctx.codeGen->CreateVar("bool", init_value ? "true" : "false", CppGen::Var::Interface));
}

void CheckBox::DoDraw(UIContext& ctx)
{
	static bool dummy;
	dummy = init_value;
	ImGui::Checkbox(text.c_str(), &dummy);
}

void CheckBox::DoExport(std::ostream& os, UIContext& ctx)
{
	os << ctx.ind;
	if (!onChange.empty())
		os << "if (";

	os << "ImGui::Checkbox("
		<< text.to_arg() << ", "
		<< "&" << field_name.c_str()
		<< ")";
	
	if (!onChange.empty()) {
		os << ")\n";
		ctx.ind_up();
		os << ctx.ind << onChange.to_arg() << "();\n";
		ctx.ind_down();
	}
	else {
		os << ";\n";
	}
}

void CheckBox::DoImport(const cpp::stmt_iterator& sit, UIContext& ctx)
{
	if ((sit->kind == cpp::CallExpr && sit->callee == "ImGui::Checkbox") ||
		(sit->kind == cpp::IfCallThenCall && sit->cond == "ImGui::Checkbox"))
	{
		if (sit->params.size())
			text.set_from_arg(sit->params[0]);
		
		if (sit->params.size() >= 2 && !sit->params[1].compare(0, 1, "&"))
		{
			field_name.set_from_arg(sit->params[1].substr(1));
			std::string fn = field_name.c_str();
			const auto* var = ctx.codeGen->GetVar(fn);
			if (!var)
				ctx.errors.push_back("CheckBox: field_name variable '" + fn + "' doesn't exist");
			else
				init_value = var->init == "true";
		}

		if (sit->kind == cpp::IfCallThenCall)
			onChange.set_from_arg(sit->callee);
	}
}

std::vector<UINode::Prop>
CheckBox::Properties()
{
	auto props = Widget::Properties();
	props.insert(props.begin(), {
		{ "text", &text },
		{ "check.init_value", &init_value },
		{ "check.field_name", &field_name },
		});
	return props;
}

bool CheckBox::PropertyUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Text("text");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##text", &text, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("text", &text, ctx);
		break;
	case 1:
		ImGui::Text("init_value");
		ImGui::TableNextColumn();
		if (ImGui::Checkbox("##init_value", init_value.access()))
		{
			changed = true;
			ctx.codeGen->ChangeVar(field_name.c_str(), "bool", init_value ? "true" : "false");
		}
		break;
	case 2:
	{
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(202, 202, 255, 255));
		ImGui::Text("field_name");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputFieldName("##field", &field_name, false, ctx);
		break;
	}
	default:
		return Widget::PropertyUI(i - 3, ctx);
	}
	return changed;
}

std::vector<UINode::Prop>
CheckBox::Events()
{
	auto props = Widget::Events();
	props.insert(props.begin(), {
		{ "onChange", &onChange }
		});
	return props;
}

bool CheckBox::EventUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Text("onChange");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##onChange", &onChange, ctx);
		break;
	default:
		return Widget::EventUI(i - 1, ctx);
	}
	return changed;
}

//-----------------------------------------------------

RadioButton::RadioButton(UIContext& ctx)
	: Widget(ctx)
{
	//variable is shared among buttons so don't generate new here
}

void RadioButton::DoDraw(UIContext& ctx)
{
	ImGui::RadioButton(text.c_str(), valueID==0);
}

void RadioButton::DoExport(std::ostream& os, UIContext& ctx)
{
	if (!ctx.codeGen->GetVar(field_name.c_str()))
		ctx.errors.push_back("RadioButon: field_name variable doesn't exist");

	os << ctx.ind << "ImGui::RadioButton("
		<< text.to_arg() << ", "
		<< "&" << field_name.c_str() << ", "
		<< valueID << ");\n";
}

void RadioButton::DoImport(const cpp::stmt_iterator& sit, UIContext& ctx)
{
	if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::RadioButton")
	{
		if (sit->params.size())
			text.set_from_arg(sit->params[0]);

		if (sit->params.size() >= 2 && !sit->params[1].compare(0, 1, "&"))
		{
			field_name.set_from_arg(sit->params[1].substr(1));
		}
	}
}

std::vector<UINode::Prop>
RadioButton::Properties()
{
	auto props = Widget::Properties();
	props.insert(props.begin(), {
		{ "text", &text },
		{ "radio.valueID", &valueID },
		{ "field_name", &field_name },
	});
	return props;
}

bool RadioButton::PropertyUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Text("text");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##text", &text, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("text", &text, ctx);
		break;
	case 1:
		ImGui::Text("valueID");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = ImGui::InputInt("##valueID", valueID.access());
		break;
	case 2:
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(202, 202, 255, 255));
		ImGui::Text("field_name");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputFieldName("##field", &field_name, false, ctx);
		break;
	default:
		return Widget::PropertyUI(i - 3, ctx);
	}
	return changed;
}

//---------------------------------------------------

Input::Input(UIContext& ctx)
	: Widget(ctx)
{
	flags.prefix("ImGuiInputTextFlags_");
	flags.add$(ImGuiInputTextFlags_CharsDecimal);
	flags.add$(ImGuiInputTextFlags_CharsHexadecimal);
	flags.add$(ImGuiInputTextFlags_CharsScientific);
	flags.add$(ImGuiInputTextFlags_CharsUppercase);
	flags.add$(ImGuiInputTextFlags_CharsNoBlank);
	flags.add$(ImGuiInputTextFlags_AutoSelectAll);
	flags.add$(ImGuiInputTextFlags_CallbackCompletion);
	flags.add$(ImGuiInputTextFlags_CallbackHistory);
	flags.add$(ImGuiInputTextFlags_CallbackAlways);
	flags.add$(ImGuiInputTextFlags_CallbackCharFilter);
	flags.add$(ImGuiInputTextFlags_CtrlEnterForNewLine);
	flags.add$(ImGuiInputTextFlags_ReadOnly);
	flags.add$(ImGuiInputTextFlags_Password);
	flags.add$(ImGuiInputTextFlags_Multiline);

	if (!ctx.importState)
		field_name.set_from_arg(ctx.codeGen->CreateVar(type, "", CppGen::Var::Interface));
}

void Input::DoDraw(UIContext& ctx)
{
	float ftmp = 0;
	int itmp = 0;
	std::string stmp;

	if (type == "int")
	{
		if (size_x.has_value())
			ImGui::SetNextItemWidth(size_x.value());
		ImGui::InputInt("", &itmp);
	}
	else if (type == "float")
	{
		if (size_x.has_value())
			ImGui::SetNextItemWidth(size_x.value());
		ImGui::InputFloat("", &ftmp);
	}
	else if (flags & ImGuiInputTextFlags_Multiline)
	{
		ImVec2 size{ 0, 0 };
		if (size_x.has_value())
			size.x = size_x.value();
		if (size_y.has_value())
			size.y = size_y.value();
		ImGui::InputTextMultiline("", &stmp, size, flags);
	}
	else
	{
		if (size_x.has_value())
			ImGui::SetNextItemWidth(size_x.value());
		if (hint != "")
			ImGui::InputTextWithHint("", hint.c_str(), &stmp, flags);
		else
			ImGui::InputText("", &stmp, flags);
	}
}

void Input::DoExport(std::ostream& os, UIContext& ctx)
{
	if (keyboard_focus)
	{
		os << ctx.ind << "if (ImGui::IsWindowAppearing())\n";
		ctx.ind_up();
		os << ctx.ind << "ImGui::SetKeyboardFocusHere();\n";
		ctx.ind_down();
	}

	os << ctx.ind << "ImGui::SetNextItemWidth(" << size_x.to_arg() << ");\n";
	
	os << ctx.ind;
	if (!onChange.empty())
		os << "if (";

	if (type == "int")
	{
		os << "ImGui::InputInt(\"##" << field_name.c_str() << "\", &"
			<< field_name.to_arg() << ")";
	}
	else if (type == "float")
	{
		os << "ImGui::InputFloat(\"##" << field_name.c_str() << "\", &" 
			<< field_name.to_arg() << ")";
	}
	else if (flags & ImGuiInputTextFlags_Multiline)
	{
		os << "ImGui::InputTextMultiline(\"##" << field_name.c_str() << "\", &" 
			<< field_name.to_arg()
			<< ", { " << size_x.to_arg() << ", " << size_y.to_arg() << " }, "
			<< flags.to_arg() << ")";
	}
	else
	{
		if (hint != "")
			os << "ImGui::InputTextWithHint(\"##" << field_name.c_str() << "\", " << hint.to_arg() << ", ";
		else
			os << "ImGui::InputText(\"##" << field_name.c_str() << "\", ";
		os << "&" << field_name.to_arg() << ", " << flags.to_arg() << ")";
	}

	if (!onChange.empty()) {
		os << ")\n";
		ctx.ind_up();
		os << ctx.ind << onChange.to_arg() << "();\n";
		ctx.ind_down();
	}
	else {
		os << ";\n";
	}
}

void Input::DoImport(const cpp::stmt_iterator& sit, UIContext& ctx)
{
	if ((sit->kind == cpp::CallExpr && sit->callee == "ImGui::InputTextMultiline") ||
		(sit->kind == cpp::IfCallThenCall && sit->cond == "ImGui::InputTextMultiline"))
	{
		type = "std::string";

		if (sit->params.size() >= 2 && !sit->params[1].compare(0, 1, "&")) {
			field_name.set_from_arg(sit->params[1].substr(1));
			std::string fn = field_name.c_str();
			if (!ctx.codeGen->GetVar(fn))
				ctx.errors.push_back("Input: field_name variable '" + fn + "' doesn't exist");
		}

		if (sit->params.size() >= 3) {
			auto size = cpp::parse_size(sit->params[2]);
			size_x = size.x;
			size_y = size.y;
		}

		if (sit->params.size() >= 4)
			flags.set_from_arg(sit->params[3]);

		if (sit->kind == cpp::IfCallThenCall)
			onChange.set_from_arg(sit->callee);
	}
	else if ((sit->kind == cpp::CallExpr && !sit->callee.compare(0, 12, "ImGui::Input")) ||
			(sit->kind == cpp::IfCallThenCall && !sit->cond.compare(0, 12, "ImGui::Input")))
	{
		if (sit->callee == "ImGui::InputInt" || sit->cond == "ImGui::InputInt")
			type = "int";
		else if (sit->callee == "ImGui::InputFloat" || sit->cond == "ImGui::InputFloat")
			type = "float";
		else
			type = "std::string";

		size_t ex = 0;
		if (sit->callee == "ImGui::InputTextWithHint" || sit->cond == "ImGui::InputTextWithHint") {
			hint = cpp::parse_str_arg(sit->params[1]);
			++ex;
		}

		if (sit->params.size() > 1 + ex && !sit->params[1 + ex].compare(0, 1, "&"))
			field_name.set_from_arg(sit->params[1 + ex].substr(1));

		if (type == "std::string" && sit->params.size() > 2 + ex)
			flags.set_from_arg(sit->params[2 + ex]);

		if (sit->kind == cpp::IfCallThenCall)
			onChange.set_from_arg(sit->callee);
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::SetNextItemWidth")
	{
		if (sit->params.size()) {
			size_x.set_from_arg(sit->params[0]);
		}
	}
	else if (sit->kind == cpp::IfCallThenCall && sit->callee == "ImGui::SetKeyboardFocusHere")
	{
		keyboard_focus = true;
	}
}

std::vector<UINode::Prop>
Input::Properties()
{
	auto props = Widget::Properties();
	props.insert(props.begin(), {
		{ "input.field_name", &field_name },
		{ "input.type", &type },
		{ "input.flags", &flags },
		{ "keyboard_focus", &keyboard_focus },
		{ "hint", &hint },
		{ "size_x", &size_x },
		{ "size_y", &size_y }, 
	});
	return props;
}

bool Input::PropertyUI(int i, UIContext& ctx)
{
	static const char* TYPES[] {
		"int",
		"float",
		"std::string"
	};

	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(202, 202, 255, 255));
		ImGui::Text("field_name");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputFieldName("##field_name", field_name.access(), type, false, ctx);
		break;
	case 1:
		ImGui::Text("type");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		if (ImGui::BeginCombo("##type", type.c_str()))
		{
			for (const auto& tp : TYPES)
			{
				if (ImGui::Selectable(tp, type == tp)) {
					changed = true;
					type = tp;
					ctx.codeGen->ChangeVar(field_name.c_str(), type, "");
				}
			}
			ImGui::EndCombo();
		}
		break;
	case 2:
		ImGui::BeginDisabled(type != "std::string");
		ImGui::Text("hint");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = ImGui::InputText("##hint", hint.access());
		ImGui::EndDisabled();
		break;
	case 3:
		ImGui::BeginDisabled(type != "std::string");
		ImGui::Unindent();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
		if (ImGui::TreeNode("flags")) {
			ImGui::TableNextColumn();
			changed = CheckBoxFlags(&flags);
			ImGui::TreePop();
		}
		ImGui::Spacing();
		ImGui::PopStyleVar();
		ImGui::Indent();
		ImGui::EndDisabled();
		break;
	case 4:
		ImGui::Text("keyboard_focus");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = ImGui::Checkbox("##kbf", keyboard_focus.access());
		break;
	case 5:
		ImGui::Text("size_x");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_x", &size_x, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_x", &size_x, ctx);
		break;
	case 6:
		ImGui::BeginDisabled(type != "std::string" || !(flags & ImGuiInputTextFlags_Multiline));
		ImGui::Text("size_y");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_y", &size_y, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_y", &size_y, ctx);
		ImGui::EndDisabled();
		break;
	default:
		return Widget::PropertyUI(i - 7, ctx);
	}
	return changed;
}

std::vector<UINode::Prop>
Input::Events()
{
	auto props = Widget::Events();
	props.insert(props.begin(), {
		{ "onChange", &onChange }
		});
	return props;
}

bool Input::EventUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Text("onChange");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##onChange", &onChange, ctx);
		break;
	default:
		return Widget::EventUI(i - 1, ctx);
	}
	return changed;
}

//---------------------------------------------------

Combo::Combo(UIContext& ctx)
	: Widget(ctx)
{
	if (!ctx.importState)
		field_name.set_from_arg(ctx.codeGen->CreateVar("int", "-1", CppGen::Var::Interface));
}

void Combo::DoDraw(UIContext& ctx)
{
	int zero = 0;
	if (size_x.has_value())
		ImGui::SetNextItemWidth(size_x.value());
	std::string label = std::string("##") + field_name.c_str();
	auto vars = items.used_variables();
	if (vars.empty())
	{
		ImGui::Combo(label.c_str(), &zero, items.c_str());
	}
	else
	{
		std::string tmp = '{' + vars[0] + "}\0";
		ImGui::Combo(label.c_str(), &zero, tmp.c_str());
	}
}

void Combo::DoExport(std::ostream& os, UIContext& ctx)
{
	if (!size_x.empty())
		os << ctx.ind << "ImGui::SetNextItemWidth(" << size_x.to_arg() << ");\n";

	os << ctx.ind;
	if (!onChange.empty())
		os << "if (";

	auto vars = items.used_variables();
	if (vars.empty())
	{
		os << "ImGui::Combo(\"##" << field_name.c_str() << "\", &"
			<< field_name.to_arg() << ", " << items.to_arg() << ")";
	}
	else
	{
		os << "ImRad::Combo(\"##" << field_name.c_str() << "\", &"
			<< field_name.to_arg() << ", " << items.to_arg() << ")";
	}

	if (!onChange.empty()) {
		os << ")\n";
		ctx.ind_up();
		os << ctx.ind << onChange.to_arg() << "();\n";
		ctx.ind_down();
	}
	else {
		os << ";\n";
	}
}

void Combo::DoImport(const cpp::stmt_iterator& sit, UIContext& ctx)
{
	if ((sit->kind == cpp::CallExpr && (sit->callee == "ImGui::Combo" || sit->callee == "ImRad::Combo")) ||
		(sit->kind == cpp::IfCallThenCall && (sit->cond == "ImGui::Combo" || sit->cond == "ImRad::Combo")))
	{
		if (sit->params.size() >= 2 && !sit->params[1].compare(0, 1, "&")) {
			field_name.set_from_arg(sit->params[1].substr(1));
			std::string fn = field_name.c_str();
			if (!ctx.codeGen->GetVar(fn))
				ctx.errors.push_back("Combo: field_name variable '" + fn + "' doesn't exist");
		}

		if (sit->params.size() >= 3) {
			items.set_from_arg(sit->params[2]);
		}

		if (sit->kind == cpp::IfCallThenCall)
			onChange.set_from_arg(sit->callee);
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::SetNextItemWidth")
	{
		if (sit->params.size()) {
			size_x.set_from_arg(sit->params[0]);
		}
	}
}

std::vector<UINode::Prop>
Combo::Properties()
{
	auto props = Widget::Properties();
	props.insert(props.begin(), {
		{ "combo.field_name", &field_name },
		{ "combo.items", &items },
		{ "size_x", &size_x },
		});
	return props;
}

bool Combo::PropertyUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(202, 202, 255, 255));
		ImGui::Text("field_name");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputFieldName("##field_name", field_name.access(), "int", false, ctx);
		break;
	case 1:
		ImGui::Text("items");
		ImGui::TableNextColumn();
		//ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		if (ImGui::Selectable("...", false, 0, { ImGui::GetContentRegionAvail().x-ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }))
		{
			changed = true;
			std::string tmp = *items.access(); //preserve embeded nulls
			stx::replace(tmp, '\0', '\n');
			comboDlg.value = tmp;
			comboDlg.OpenPopup([this](ImRad::ModalResult) {
				std::string tmp = comboDlg.value;
				if (!tmp.empty() && tmp.back() != '\n')
					tmp.push_back('\n');
				stx::replace(tmp, '\n', '\0');
				*items.access() = tmp;
				});
		}
		ImGui::SameLine(0, 0);
		BindingButton("items", &items, ctx);
		break;
	case 2:
		ImGui::Text("size_x");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_x", &size_x, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_x", &size_x, ctx);
		break;
	default:
		return Widget::PropertyUI(i - 3, ctx);
	}
	return changed;
}

std::vector<UINode::Prop>
Combo::Events()
{
	auto props = Widget::Events();
	props.insert(props.begin(), {
		{ "onChange", &onChange }
		});
	return props;
}

bool Combo::EventUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
		ImGui::Text("onChange");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-1);
		changed = InputEvent("##onChange", &onChange, ctx);
		break;
	default:
		return Widget::EventUI(i - 1, ctx);
	}
	return changed;
}

//---------------------------------------------------

Table::ColumnData::ColumnData()
{
	flags.prefix("ImGuiTableColumnFlags_");
	flags.add$(ImGuiTableColumnFlags_Disabled);
	flags.add$(ImGuiTableColumnFlags_WidthStretch);
	flags.add$(ImGuiTableColumnFlags_WidthFixed);
	flags.add$(ImGuiTableColumnFlags_NoResize);
	flags.add$(ImGuiTableColumnFlags_NoHide);
}

Table::Table(UIContext& ctx)
	: Widget(ctx)
{
	flags.prefix("ImGuiTableFlags_");
	flags.add$(ImGuiTableFlags_Resizable);
	flags.add$(ImGuiTableFlags_Reorderable);
	flags.add$(ImGuiTableFlags_Hideable);
	flags.add$(ImGuiTableFlags_ContextMenuInBody);
	flags.separator();
	flags.add$(ImGuiTableFlags_RowBg);
	flags.add$(ImGuiTableFlags_BordersInnerH);
	flags.add$(ImGuiTableFlags_BordersInnerV);
	flags.add$(ImGuiTableFlags_BordersOuterH);
	flags.add$(ImGuiTableFlags_BordersOuterV);
	//flags.add$(ImGuiTableFlags_NoBordersInBody);
	//flags.add$(ImGuiTableFlags_NoBordersInBodyUntilResize);
	flags.separator();
	flags.add$(ImGuiTableFlags_SizingFixedFit);
	flags.add$(ImGuiTableFlags_SizingFixedSame);
	flags.add$(ImGuiTableFlags_SizingStretchProp);
	flags.add$(ImGuiTableFlags_SizingStretchSame);
	flags.separator();
	flags.add$(ImGuiTableFlags_PadOuterX);
	flags.add$(ImGuiTableFlags_NoPadOuterX);
	flags.add$(ImGuiTableFlags_NoPadInnerX);
	flags.add$(ImGuiTableFlags_ScrollX);
	flags.add$(ImGuiTableFlags_ScrollY);

	columnData.resize(3);
	for (size_t i = 0; i < columnData.size(); ++i)
		columnData[i].label = char('A' + i);
}

void Table::DoDraw(UIContext& ctx)
{
	int n = std::max(1, (int)columnData.size());
	if (!stylePadding)
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 1, 1 });
	ImVec2 size{ 0, 0 };
	if (size_x.has_value())
		size.x = size_x.value();
	if (size_y.has_value())
		size.y = size_y.value();
	if (ImGui::BeginTable(("table" + std::to_string((uint64_t)this)).c_str(), n, flags, size))
	{
		for (size_t i = 0; i < (int)columnData.size(); ++i)
			ImGui::TableSetupColumn(columnData[i].label.c_str(), columnData[i].flags, columnData[i].width);
		if (header)
			ImGui::TableHeadersRow();
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		
		bool tmp = ctx.table;
		ctx.table = true;
		for (int i = 0; i < (int)children.size(); ++i)
		{
			children[i]->Draw(ctx);
			//ImGui::Text("cell");
		}
		ctx.table = tmp;

		ImGui::EndTable();
	}
	if (!stylePadding)
		ImGui::PopStyleVar();
}

std::vector<UINode::Prop>
Table::Properties()
{
	auto props = Widget::Properties();
	props.insert(props.begin(), {
		{ "table.columns", nullptr },
		{ "table.row_count", &row_count },
		{ "table.flags", &flags },
		{ "style_padding", &stylePadding },
		{ "table.header", &header },
		{ "size_x", &size_x },
		{ "size_y", &size_y },
		});
	return props;
}

bool Table::PropertyUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	case 0:
	{
		ImGui::Text("columns");
		ImGui::TableNextColumn();
		if (ImGui::Selectable("...", false, 0, { ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight(), ImGui::GetFrameHeight() }))
		{
			changed = true;
			tableColumns.columnData = columnData;
			tableColumns.target = &columnData;
			tableColumns.OpenPopup();
		}
		/*auto tmp = std::to_string(columnData.size());
		ImGui::SetNextItemWidth(-2 * ImGui::GetFrameHeight());
		ImGui::InputText("##columns", &tmp, ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine(0, 0);
		if (ImGui::Button("..."))
		{
			changed = true;
			tableColumns.columnData = columnData;
			tableColumns.target = &columnData;
			tableColumns.OpenPopup();
		}*/
		break;
	}
	case 1:
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(202, 202, 255, 255));
		ImGui::Text("row_count");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputFieldName("##row_count", &row_count, true, ctx);
		break;
	case 2:
		ImGui::Unindent();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
		if (ImGui::TreeNode("flags"))
		{
			ImGui::TableNextColumn();
			changed = CheckBoxFlags(&flags);
			ImGui::TreePop();
		}
		ImGui::Spacing();
		ImGui::PopStyleVar();
		ImGui::Indent();
		break;
	case 3:
		ImGui::Text("style_padding");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##style_padding", stylePadding.access());
		break;
	case 4:
		ImGui::Text("header");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##header", header.access());
		break;
	case 5:
		ImGui::Text("size_x");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_x", &size_x, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_x", &size_x, ctx);
		break;
	case 6:
		ImGui::Text("size_y");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_y", &size_y, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_y", &size_y, ctx);
		break;
	default:
		return Widget::PropertyUI(i - 7, ctx);
	}
	return changed;
}

void Table::DoExport(std::ostream& os, UIContext& ctx)
{
	if (!stylePadding)
		os << ctx.ind << "ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 1, 1 });\n";
	
	os << ctx.ind << "if (ImGui::BeginTable("
		<< "\"table" << (uint64_t)this << "\", " 
		<< columnData.size() << ", " 
		<< flags.to_arg() << ", "
		<< "{ " << size_x.to_arg() << ", " << size_y.to_arg() << " }"
		<< "))\n"
		<< ctx.ind << "{\n";
	ctx.ind_up();

	for (const auto& cd : columnData)
	{
		os << ctx.ind << "ImGui::TableSetupColumn(\"" << cd.label << "\", "
			<< cd.flags.to_arg() << ", " << cd.width << ");\n";
	}
	if (header)
		os << ctx.ind << "ImGui::TableHeadersRow();\n";

	bool tmp = ctx.table;
	ctx.table = true;
	if (!row_count.empty()) {
		os << "\n" << ctx.ind << "for (size_t " << FOR_VAR << " = 0; " << FOR_VAR 
			<< " < " << row_count.to_arg() << "; ++" << FOR_VAR
			<< ")\n" << ctx.ind << "{\n";
		ctx.ind_up();
		os << ctx.ind << "ImGui::TableNextRow();\n";
		os << ctx.ind << "ImGui::TableSetColumnIndex(0);\n";
		os << ctx.ind << "ImGui::PushID((int)" << FOR_VAR << ");\n";
	}
	else {
		os << ctx.ind << "ImGui::TableNextRow();\n";
		os << ctx.ind << "ImGui::TableSetColumnIndex(0);\n";
	}
	
	os << ctx.ind << "/// @separator\n\n";

	for (auto& child : children)
		child->Export(os, ctx);

	os << "\n" << ctx.ind << "/// @separator\n";

	if (!row_count.empty()) {
		os << ctx.ind << "ImGui::PopID();\n";
		ctx.ind_down();
		os << ctx.ind << "}\n";
	}
	ctx.table = tmp;

	os << ctx.ind << "ImGui::EndTable();\n";
	ctx.ind_down();
	os << ctx.ind << "}\n";

	if (!stylePadding)
		os << ctx.ind << "ImGui::PopStyleVar();\n";
}

void Table::DoImport(const cpp::stmt_iterator& sit, UIContext& ctx)
{
	if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::PushStyleVar")
	{
		if (sit->params.size() && sit->params[0] == "ImGuiStyleVar_CellPadding")
			stylePadding = false;
	}
	else if (sit->kind == cpp::IfCallBlock && sit->callee == "ImGui::BeginTable")
	{
		header = false;
		ctx.importLevel = 0;

		if (sit->params.size() >= 2) {
			std::istringstream is(sit->params[1]);
			size_t n = 3;
			is >> n;
			columnData.resize(n);
		}

		if (sit->params.size() >= 3)
			flags.set_from_arg(sit->params[2]);

		if (sit->params.size() >= 4) {
			auto size = cpp::parse_size(sit->params[3]);
			size_x = size.x;
			size_y = size.y;
		}
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::TableSetupColumn")
	{
		ColumnData cd;
		cd.label = cpp::parse_str_arg(sit->params[0]);
		
		if (sit->params.size() >= 2)
			cd.flags.set_from_arg(sit->params[1]);
		
		if (sit->params.size() >= 3) {
			std::istringstream is(sit->params[2]);
			is >> cd.width;
		}
		
		columnData[ctx.importLevel++] = std::move(cd);
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::TableHeadersRow")
	{
		header = true;
	}
	else if (sit->kind == cpp::ForBlock)
	{
		if (!sit->cond.compare(0, FOR_VAR.size()+1, std::string(FOR_VAR) + "<")) //VS bug without std::string()
			row_count.set_from_arg(sit->cond.substr(FOR_VAR.size() + 1));
	}
}

//---------------------------------------------------------

Child::Child(UIContext& ctx)
	: Widget(ctx)
{
}

void Child::DoDraw(UIContext& ctx)
{
	if (!stylePadding)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (styleBg.has_value())
		ImGui::PushStyleColor(ImGuiCol_ChildBg, styleBg.value());

	ImVec2 sz(0, 0);
	if (size_x.has_value()) 
		sz.x = size_x.value();
	if (size_y.has_value())
		sz.y = size_y.value();
	if (!sz.x && children.empty())
		sz.x = 30;
	if (!sz.y && children.empty())
		sz.y = 30;
	//very weird - using border controls window padding
	ImGui::BeginChild("", sz, border); 
	//draw border for visual distinction
	if (!border && !styleBg.has_value()) {
		ImDrawList* dl = ImGui::GetWindowDrawList();
		auto clr = ImGui::GetStyle().Colors[ImGuiCol_Border];
		dl->AddRect(cached_pos, cached_pos + cached_size, ImGui::ColorConvertFloat4ToU32(clr), 0, 0, 1);
	}

	if (column_count.has_value() && column_count.value() >= 2)
	{
		int n = column_count.value();
		//ImGui::SetColumnWidth doesn't support auto size columns
		//We compute it ourselves
		float fixedWidth = (n - 1) * 10.f; //spacing
		int autoSized = 0;
		for (size_t i = 0; i < columnsWidths.size(); ++i)
		{
			if (columnsWidths[i].has_value() && columnsWidths[i].value())
				fixedWidth += columnsWidths[i].value();
			else
				++autoSized;
		}
		float autoSizeW = (ImGui::GetContentRegionAvail().x - fixedWidth) / autoSized;
		
		ImGui::Columns(n, "columns", column_border);
		for (int i = 0; i < (int)columnsWidths.size(); ++i)
		{
			if (columnsWidths[i].has_value() && columnsWidths[i].value())
				ImGui::SetColumnWidth(i, columnsWidths[i].value());
			else
				ImGui::SetColumnWidth(i, autoSizeW);
		}
	}

	for (size_t i = 0; i < children.size(); ++i)
	{
		children[i]->Draw(ctx);
	}
		
	ImGui::EndChild();

	if (styleBg.has_value())
		ImGui::PopStyleColor();
	if (!stylePadding)
		ImGui::PopStyleVar();
}

void Child::DoExport(std::ostream& os, UIContext& ctx)
{
	if (!stylePadding)
		os << ctx.ind << "ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });\n";
	if (!styleBg.empty())
		os << ctx.ind << "ImGui::PushStyleColor(ImGuiCol_ChildBg, " << styleBg.to_arg() << ");\n";

	os << ctx.ind << "ImGui::BeginChild(\"child" << (uint64_t)this << "\", "
		<< "{ " << size_x.to_arg() << ", " << size_y.to_arg() << " }, "
		<< std::boolalpha << border
		<< ");\n";
	os << ctx.ind << "{\n";
	ctx.ind_up();

	bool hasColumns = !column_count.has_value() || column_count.value() >= 2;
	if (hasColumns) {
		os << ctx.ind << "ImGui::Columns(" << column_count.to_arg() << ", \"\", "
			<< column_border.to_arg() << ");\n";
		//for (size_t i = 0; i < columnsWidths.size(); ++i)
			//os << ctx.ind << "ImGui::SetColumnWidth(" << i << ", " << columnsWidths[i].c_str() << ");\n";
	}

	if (!data_size.empty()) {
		os << ctx.ind << "for (size_t " << FOR_VAR << " = 0; " << FOR_VAR
			<< " < " << data_size.to_arg() << "; ++" << FOR_VAR
			<< ")\n" << ctx.ind << "{\n";
		ctx.ind_up();
	}

	os << ctx.ind << "/// @separator\n\n";

	for (auto& child : children)
		child->Export(os, ctx);

	os << ctx.ind << "/// @separator\n";

	if (!data_size.empty()) {
		if (hasColumns)
			os << ctx.ind << "ImGui::NextColumn();\n";
		ctx.ind_down();
		os << ctx.ind << "}\n";
	}

	os << ctx.ind << "ImGui::EndChild();\n";
	ctx.ind_down();
	os << ctx.ind << "}\n";

	if (!styleBg.empty())
		os << ctx.ind << "ImGui::PopStyleColor();\n";
	if (!stylePadding)
		os << ctx.ind << "ImGui::PopStyleVar();\n";
}

void Child::DoImport(const cpp::stmt_iterator& sit, UIContext& ctx)
{
	if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::PushStyleColor")
	{
		if (sit->params.size() == 2 && sit->params[0] == "ImGuiCol_ChildBg")
			styleBg.set_from_arg(sit->params[1]);
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::PushStyleVar")
	{
		if (sit->params.size() == 2 &&
			sit->params[0] == "ImGuiStyleVar_WindowPadding" &&
			!Norm(cpp::parse_size(sit->params[1])))
				stylePadding = false;
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::BeginChild")
	{
		if (sit->params.size() >= 2) {
			auto size = cpp::parse_size(sit->params[1]);
			size_x = size.x;
			size_y = size.y;
		}

		if (sit->params.size() >= 3)
			border = sit->params[2] == "true";
	}
	else if (sit->kind == cpp::CallExpr && sit->callee == "ImGui::Columns")
	{
		if (sit->params.size())
			*column_count.access() = sit->params[0];

		if (sit->params.size() >= 3)
			column_border = sit->params[2] == "true";

		if (column_count.has_value())
			columnsWidths.resize(column_count.value(), 0.f);
		else
			columnsWidths.resize(1, 0.f);
	}
	else if (sit->kind == cpp::ForBlock)
	{
		if (!sit->cond.compare(0, FOR_VAR.size() + 1, FOR_VAR + "<"))
			data_size.set_from_arg(sit->cond.substr(FOR_VAR.size() + 1));
	}
}


std::vector<UINode::Prop>
Child::Properties()
{
	auto props = Widget::Properties();
	props.insert(props.begin(), {
		//{ "style_padding", &stylePadding }, padding is already controlled by border
		{ "style_color", &styleBg },
		{ "border", &border },
		{ "size_x", &size_x },
		{ "size_y", &size_y },
		{ "child.column_count", &column_count },
		{ "child.column_border", &column_border },
		{ "child.data_size", &data_size },
		});
	return props;
}

bool Child::PropertyUI(int i, UIContext& ctx)
{
	bool changed = false;
	switch (i)
	{
	/*case 0:
		ImGui::Text("style_padding");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##padding", stylePadding.access());
		break;*/
	case 0:
		ImGui::Text("style_color");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##color", &styleBg, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("style_color", &styleBg, ctx);
		break;
	case 1:
		ImGui::Text("border");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##border", border.access());
		break;
	case 2:
		ImGui::Text("size_x");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_x", &size_x, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_x", &size_x, ctx);
		break;
	case 3:
		ImGui::Text("size_y");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##size_y", &size_y, 0.f, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("size_y", &size_y, ctx);
		break;
	case 4:
		ImGui::Text("column_count");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputBindable("##column_count", &column_count, 1, ctx);
		ImGui::SameLine(0, 0);
		BindingButton("column_count", &column_count, ctx);
		break;
	case 5:
		ImGui::Text("column_border");
		ImGui::TableNextColumn();
		changed = ImGui::Checkbox("##column_border", column_border.access());
		break;
	case 6:
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(202, 202, 255, 255));
		ImGui::Text("data_size");
		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-ImGui::GetFrameHeight());
		changed = InputFieldName("##data_size", &data_size, true, ctx);
		break;
	default:
		return Widget::PropertyUI(i - 7, ctx);
	}
	return changed;
}
