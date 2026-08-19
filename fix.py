import re

with open('src/TablePages.cpp', 'r') as f:
    text = f.read()

# Replace timers
pattern = re.compile(r'QTimer::singleShot\(50,\s*this,\s*\[([^\]]*)\]\(\)\s*\{\s*(.*?)\s*(?://.*?)?\s*QTimer::singleShot\(500,\s*this,\s*\[this\]\(\)\s*\{\s*refresh\(\);\s*pimpl_->overlay_->hide\(\);\s*\}\);\s*\}\);', re.DOTALL)
text = pattern.sub(r'\2', text)

# Function to inject connections
def inject_connections(class_name, text):
    find_str = f'QTimer::singleShot(100, this, &{class_name}::refresh);\n}}'
    replace_str = f'''connect(&Backend::get_instance(), &Backend::cli_action_success, this, [this](const QString&) {{
        pimpl_->overlay_->hide();
        refresh();
    }});
    connect(&Backend::get_instance(), &Backend::cli_error_occurred, this, [this](const QString&) {{
        pimpl_->overlay_->hide();
        refresh();
    }});
    
    QTimer::singleShot(100, this, &{class_name}::refresh);
}}'''
    return text.replace(find_str, replace_str)

text = inject_connections('ContainersPage', text)
text = inject_connections('ImagesPage', text)
text = inject_connections('VolumesPage', text)
text = inject_connections('PortsPage', text)

with open('src/TablePages.cpp', 'w') as f:
    f.write(text)
