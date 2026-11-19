#pragma once

#include <QRegularExpression>
#include <QStringList>

namespace SearchMatcher {
inline QString normalized(const QString &text) {
    QString result;
    for (const auto character : text.normalized(QString::NormalizationForm_D)) {
        const auto category = character.category();
        if (category != QChar::Mark_NonSpacing && category != QChar::Mark_SpacingCombining &&
            category != QChar::Mark_Enclosing) result.append(character);
    }
    return result.toCaseFolded();
}

inline int score(const QString &query, const QString &candidate) {
    static const QRegularExpression separators(QStringLiteral(R"([^\p{L}\p{N}]+)"));
    const auto queryWords = normalized(query).split(separators, Qt::SkipEmptyParts);
    const auto words = normalized(candidate).split(separators, Qt::SkipEmptyParts);
    if (queryWords.isEmpty() || words.isEmpty()) return 0;
    const auto needle = queryWords.join(QLatin1Char(' '));
    const auto text = words.join(QLatin1Char(' '));
    if (text.startsWith(needle)) return 1000 - qMin(text.size(), 100);
    const auto substring = text.indexOf(needle);
    if (substring >= 0) return 900 - qMin(substring * 3 + text.size() / 8, 150);
    if (queryWords.size() > 1) {
        int position = 0;
        int gaps = 0;
        for (const auto &part : queryWords) {
            while (position < words.size() && !words.at(position).startsWith(part)) {
                ++position;
                ++gaps;
            }
            if (position == words.size()) return 0;
            ++position;
        }
        return 850 - qMin(gaps * 10, 100);
    }
    const auto compactQuery = queryWords.first();
    QString initials;
    for (const auto &word : words) initials.append(word.front());
    if (initials.startsWith(compactQuery)) return 825 - qMin(words.size(), 50);
    for (int index = 1; index < words.size(); ++index)
        if (words.at(index).startsWith(compactQuery)) return 780 - qMin(index * 10, 100);
    int position = 0;
    int gaps = 0;
    const auto compactText = words.join(QString());
    for (const auto character : compactQuery) {
        const auto found = compactText.indexOf(character, position);
        if (found < 0) return 0;
        gaps += found - position;
        position = found + 1;
    }
    return qMax(1, 400 - gaps * 4);
}
}
