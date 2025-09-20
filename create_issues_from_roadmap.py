import requests
import re

# Konfiguration
GITHUB_TOKEN = "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"
REPO = "S540d/Multidisplay_ESP32_Rev2"  # owner/repo
ROADMAP_PATH = "ROADMAP.md"

def extract_issues_from_roadmap(path):
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    # Nur Zeilen mit [ ] als Issue nehmen
    issues = re.findall(r"- \[ \] (.+)", content)
    return issues

def create_github_issue(title, body="Siehe Roadmap.md", labels=None):
    url = f"https://api.github.com/repos/{REPO}/issues"
    headers = {
        "Authorization": f"token {GITHUB_TOKEN}",
        "Accept": "application/vnd.github+json"
    }
    data = {
        "title": title,
        "body": body,
        "labels": labels or ["roadmap"]
    }
    response = requests.post(url, json=data, headers=headers)
    if response.status_code == 201:
        print(f"Issue erstellt: {title}")
    else:
        print(f"Fehler bei {title}: {response.text}")

if __name__ == "__main__":
    issues = extract_issues_from_roadmap(ROADMAP_PATH)
    for issue in issues:
        create_github_issue(issue)