#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <gtk/gtk.h>

namespace {

constexpr int kRefreshSeconds = 5;
constexpr int kBarWidth = 32;

std::atomic<bool> g_done{false};
std::mutex g_input_mutex;
std::string g_user_input;
std::string g_file_content;
std::string g_file_display_name;

std::string ReadFileUtf8(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return "Unable to open file.";
    }

    std::string bytes((std::istreambuf_iterator<char>(input)),
                      std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        return bytes.substr(3);
    }
    return bytes;
}

std::string RenderProgressLine(const std::string& value) {
    double ratio = 0.0;
    try {
        ratio = std::stod(value) / 100.0;
    } catch (...) {
        ratio = 0.0;
    }
    if (ratio > 1.0) {
        ratio = 1.0;
    }

    int filled = static_cast<int>(ratio * kBarWidth);
    if (filled > kBarWidth) {
        filled = kBarWidth;
    }

    return "\r[" + std::string(filled, '=') + std::string(kBarWidth - filled, '.') + "] " + value + "%";
}

class ProgressGenerator {
public:
    std::string Next() {
        if (integer_part_ <= 99) {
            return std::to_string(integer_part_++);
        }

        if (suffix_.empty() || index_ >= 9) {
            suffix_.push_back('9');
            index_ = 0;
        }
        return "99." + suffix_ + digits_[index_++];
    }

private:
    int integer_part_ = 1;
    std::string suffix_;
    std::size_t index_ = 0;
    const std::string digits_ = "123456789";
};

void PrintProgress() {
    ProgressGenerator generator;

    while (!g_done.load()) {
        std::cout << RenderProgressLine(generator.Next()) << std::flush;
        for (int i = 0; i < kRefreshSeconds * 10; ++i) {
            if (g_done.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << std::endl;
}

bool SaveDisplayedText(std::string* saved_path) {
    try {
        std::filesystem::path save_dir = std::filesystem::current_path() / "aimonitor_save";
        std::filesystem::create_directories(save_dir);

        std::string file_name = g_file_display_name.empty() ? "aimonitor_saved.txt" : g_file_display_name;
        std::filesystem::path output_path = save_dir / file_name;

        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            return false;
        }
        output << g_file_content;
        output.close();

        if (!output) {
            return false;
        }

        if (saved_path != nullptr) {
            *saved_path = output_path.string();
        }
        return true;
    } catch (...) {
        return false;
    }
}

struct AppWidgets {
    GtkApplication* app;
    GtkWidget* window;
    GtkTextBuffer* text_buffer;
    GtkEntryBuffer* entry_buffer;
};

void ShowMessage(GtkWindow* parent, const std::string& title, const std::string& body) {
    GtkWidget* dialog = gtk_message_dialog_new(
        parent,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "%s",
        body.c_str());
    gtk_window_set_title(GTK_WINDOW(dialog), title.c_str());
    g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* dlg, int, gpointer) {
        gtk_window_destroy(GTK_WINDOW(dlg));
    }), nullptr);
    gtk_widget_show(dialog);
}

void OnSaveClicked(GtkButton*, gpointer user_data) {
    auto* widgets = static_cast<AppWidgets*>(user_data);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(widgets->text_buffer, &start, &end);
    char* text = gtk_text_buffer_get_text(widgets->text_buffer, &start, &end, FALSE);
    g_file_content = text != nullptr ? text : "";
    g_free(text);

    std::string saved_path;
    if (SaveDisplayedText(&saved_path)) {
        ShowMessage(GTK_WINDOW(widgets->window), "Saved", "Text saved to:\n" + saved_path);
    } else {
        ShowMessage(GTK_WINDOW(widgets->window), "Save Failed", "Failed to save the current text.");
    }
}

void FinishAndExit(AppWidgets* widgets) {
    const char* entry_text = gtk_entry_buffer_get_text(widgets->entry_buffer);
    {
        std::lock_guard<std::mutex> lock(g_input_mutex);
        g_user_input = entry_text != nullptr ? entry_text : "";
    }
    g_done.store(true);
    gtk_window_destroy(GTK_WINDOW(widgets->window));
    g_application_quit(G_APPLICATION(widgets->app));
}

void OnConfirmClicked(GtkButton*, gpointer user_data) {
    FinishAndExit(static_cast<AppWidgets*>(user_data));
}

void OnEntryActivated(GtkEntry*, gpointer user_data) {
    FinishAndExit(static_cast<AppWidgets*>(user_data));
}

gboolean OnWindowClose(GtkWindow*, gpointer user_data) {
    auto* widgets = static_cast<AppWidgets*>(user_data);
    g_done.store(true);
    g_application_quit(G_APPLICATION(widgets->app));
    return FALSE;
}

void Activate(GtkApplication* app, gpointer user_data) {
    auto* widgets = static_cast<AppWidgets*>(user_data);
    widgets->app = app;

    GtkWidget* window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "AgentCarryOn");
    gtk_window_set_default_size(GTK_WINDOW(window), 760, 560);
    widgets->window = window;

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(root, 12);
    gtk_widget_set_margin_bottom(root, 12);
    gtk_widget_set_margin_start(root, 12);
    gtk_widget_set_margin_end(root, 12);
    gtk_window_set_child(GTK_WINDOW(window), root);

    std::string label_text = g_file_display_name.empty()
        ? "No file provided. The text area is empty."
        : "Viewing: " + g_file_display_name;
    GtkWidget* file_label = gtk_label_new(label_text.c_str());
    gtk_label_set_xalign(GTK_LABEL(file_label), 0.0f);
    gtk_box_append(GTK_BOX(root), file_label);

    GtkWidget* scroller = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_append(GTK_BOX(root), scroller);

    GtkTextBuffer* text_buffer = gtk_text_buffer_new(nullptr);
    gtk_text_buffer_set_text(text_buffer, g_file_content.c_str(), -1);
    widgets->text_buffer = text_buffer;

    GtkWidget* text_view = gtk_text_view_new_with_buffer(text_buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), text_view);

    GtkWidget* input_label = gtk_label_new("Enter instructions for the agent:");
    gtk_label_set_xalign(GTK_LABEL(input_label), 0.0f);
    gtk_box_append(GTK_BOX(root), input_label);

    GtkEntryBuffer* entry_buffer = gtk_entry_buffer_new(nullptr, -1);
    widgets->entry_buffer = entry_buffer;
    GtkWidget* entry = gtk_entry_new_with_buffer(entry_buffer);
    gtk_box_append(GTK_BOX(root), entry);

    GtkWidget* button_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(button_row, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(root), button_row);

    GtkWidget* save_button = gtk_button_new_with_label("Save Text");
    GtkWidget* confirm_button = gtk_button_new_with_label("Confirm");
    gtk_box_append(GTK_BOX(button_row), save_button);
    gtk_box_append(GTK_BOX(button_row), confirm_button);

    g_signal_connect(save_button, "clicked", G_CALLBACK(OnSaveClicked), widgets);
    g_signal_connect(confirm_button, "clicked", G_CALLBACK(OnConfirmClicked), widgets);
    g_signal_connect(entry, "activate", G_CALLBACK(OnEntryActivated), widgets);
    g_signal_connect(window, "close-request", G_CALLBACK(OnWindowClose), widgets);

    gtk_widget_grab_focus(entry);
    gtk_window_present(GTK_WINDOW(window));
}

}  // namespace

int main(int argc, char* argv[]) {
    setenv("GSK_RENDERER", "cairo", 0);
    setenv("GDK_DISABLE", "vulkan", 0);

    const char* app_argv[] = {argc > 0 ? argv[0] : "aimonitor", nullptr};
    int app_argc = 1;

    if (argc >= 2) {
        std::filesystem::path input_path(argv[1]);
        g_file_display_name = input_path.filename().string();
        g_file_content = ReadFileUtf8(input_path);
    }

    std::thread progress_thread(PrintProgress);

    AppWidgets widgets{};
    GtkApplication* app = gtk_application_new("com.agentcarryon.aimonitor", G_APPLICATION_DEFAULT_FLAGS);
    widgets.app = app;
    g_signal_connect(app, "activate", G_CALLBACK(Activate), &widgets);
    int status = g_application_run(G_APPLICATION(app), app_argc, const_cast<char**>(app_argv));
    g_object_unref(app);

    g_done.store(true);
    if (progress_thread.joinable()) {
        progress_thread.join();
    }

    std::string user_input;
    {
        std::lock_guard<std::mutex> lock(g_input_mutex);
        user_input = g_user_input;
    }
    if (!user_input.empty()) {
        std::cout << user_input << std::endl;
    }

    return status;
}
