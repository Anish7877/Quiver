import re

with open("src/Backend.cpp", "r") as f:
    content = f.read()

def replace_action(action):
    global content
    pattern = rf'auto Backend::{action}_container\(const QStringList& container_ids\) -> void \{{\n\s*if \(container_ids\.isEmpty\(\)\) return;\n\s*QString cli_path = resolve_cli_path\(\);\n\s*if \(QFile::exists\(cli_path\)\) \{{\n.*?process->start\(cli_path, QStringList\(\) << "{action}" << container_ids\);\n\s*\}} else \{{\n\s*qDebug\(\) << "CLI not found at" << cli_path;\n\s*\}}\n\}}'
    
    replacement = f'''auto Backend::{action}_container(const QStringList& container_ids) -> void {{
    if (container_ids.isEmpty()) return;
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {{
        qDebug() << "Executing Quiver CLI for {action}:" << cli_path << "{action}" << container_ids;
        QProcess::startDetached(cli_path, QStringList() << "{action}" << container_ids);
    }} else {{
        qDebug() << "CLI not found at" << cli_path;
    }}
}}'''
    content = re.sub(pattern, replacement, content, flags=re.DOTALL)

for a in ["delete", "pause", "unpause", "start", "stop"]:
    replace_action(a)
    
# prune_containers
pattern_prune = r'auto Backend::prune_containers\(\) -> void \{\n\s*QString cli_path = resolve_cli_path\(\);\n\s*if \(QFile::exists\(cli_path\)\) \{\n.*?process->start\(cli_path, QStringList\(\) << "prune"\);\n\s*\} else \{\n\s*qDebug\(\) << "CLI not found at" << cli_path;\n\s*\}\n\}'
replacement_prune = '''auto Backend::prune_containers() -> void {
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        qDebug() << "Executing Quiver CLI for prune:" << cli_path << "prune";
        QProcess::startDetached(cli_path, QStringList() << "prune");
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}'''
content = re.sub(pattern_prune, replacement_prune, content, flags=re.DOTALL)

with open("src/Backend.cpp", "w") as f:
    f.write(content)
