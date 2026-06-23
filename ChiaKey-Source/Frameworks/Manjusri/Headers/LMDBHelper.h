/*
Copyright (c) 2012, Yahoo! Inc.  All rights reserved.
Copyrights licensed under the New BSD License. See the accompanying LICENSE
file for terms.
*/

#ifndef LMDBHelper_h
#define LMDBHelper_h

#ifndef OV_USE_SQLITE
#define OV_USE_SQLITE
#endif

#if defined(__APPLE__)
#include <OpenVanilla/OpenVanilla.h>
#else
#include "OpenVanilla.h"
#endif

#include "BPMFUserPhraseHelper.h"

namespace Manjusri {
using namespace OpenVanilla;

class LMDBHelper {
 public:
  LMDBHelper(OVSQLiteConnection* connection, bool ownsConnection = true)
      : m_connection(connection),
        m_ownsConnection(ownsConnection),
        m_maxUnigramProbability(0.0),
        m_minUnigramProbability(0.0),
        m_UNKProbability(0.0) {
    if (!m_connection->hasTable("cooked_information")) {
      cerr << "doesn't have table: cooked_information" << endl;
      if (m_connection->createTable("cooked_information"))
        cerr << "created table: cooked_information" << endl;
      else
        cerr << "failed to create table: cooked_information" << endl;
    }

    OVSQLiteStatement* selectMax =
        m_connection->prepare("SELECT MAX(probability) from unigrams");
    if (selectMax) {
      while (selectMax->step() == SQLITE_ROW) {
        m_maxUnigramProbability = selectMax->doubleOfColumn(0);
        // cerr << "max probability: " << m_maxUnigramProbability << endl;
      }
      delete selectMax;
    }

    // make it non-zero
    if (m_maxUnigramProbability == 0.0) m_maxUnigramProbability = -0.0000001;

    OVSQLiteStatement* selectMin =
        m_connection->prepare("SELECT MIN(probability) from unigrams");
    if (selectMin) {
      while (selectMin->step() == SQLITE_ROW) {
        m_minUnigramProbability = selectMin->doubleOfColumn(0);
      }
      delete selectMin;
    }

    // make it non-zero
    if (m_minUnigramProbability == 0.0) m_minUnigramProbability = -99.0;

    OVSQLiteStatement* findUNK = m_connection->prepare(
        "SELECT probability FROM unigrams WHERE qstring = %Q", "*");
    if (findUNK) {
      while (findUNK->step() == SQLITE_ROW) {
        m_UNKProbability = findUNK->doubleOfColumn(0);
      }
      delete findUNK;
    }
  }

  ~LMDBHelper() {
    if (m_ownsConnection && m_connection) delete m_connection;
  }

  const string version() {
    string result;

    OVSQLiteStatement* statement = m_connection->prepare(
        "SELECT value FROM cooked_information WHERE key = ?");
    if (!statement) return result;

    statement->bindTextToColumn("version", 1);
    if (statement->step() == SQLITE_ROW) {
      result = statement->textOfColumn(0);
      while (statement->step() == SQLITE_ROW)
        ;
    }

    delete statement;
    return result;
  }

  void setVersion(const string& version) {
    if (m_connection->execute("DELETE FROM cooked_information WHERE KEY = %Q",
                              "version") != SQLITE_OK) {
      cerr << "serVersion error" << endl;
      return;
    }

    if (m_connection->execute("INSERT INTO cooked_information VALUES(%Q, %Q)",
                              "version", version.c_str()) != SQLITE_OK) {
      cerr << "serVersion error" << endl;
      return;
    }
  }

  size_t countUnigramOccurances(const string& phrase) {
    size_t occurances = 0;

    OVSQLiteStatement* statement = m_connection->prepare(
        "SELECT COUNT(*) FROM unigrams WHERE current = ?");
    if (!statement) return occurances;

    statement->bindTextToColumn(phrase, 1);
    if (statement->step() == SQLITE_ROW) {
      occurances = (size_t)statement->intOfColumn(0);
      while (statement->step() == SQLITE_ROW)
        ;
    }
    delete statement;

    return occurances;
  }

  size_t countUnigramOccurancesWithBPMF(const string& phrase,
                                        const string& bpmf) {
    size_t occurances = 0;
    string qstring = BPMFUserPhraseHelper::QString(bpmf).first;

    OVSQLiteStatement* statement = m_connection->prepare(
        "SELECT COUNT(*) FROM unigrams WHERE current = ? and qstring = ?");
    if (!statement) return occurances;

    statement->bindTextToColumn(phrase, 1);
    statement->bindTextToColumn(qstring, 2);
    if (statement->step() == SQLITE_ROW) {
      occurances = (size_t)statement->intOfColumn(0);
      while (statement->step() == SQLITE_ROW)
        ;
    }
    delete statement;

    return occurances;
  }

  size_t countBigramOccurances(const string& phrase) {
    size_t occurances = 0;

    OVSQLiteStatement* statement;
    statement =
        m_connection->prepare("SELECT COUNT(*) FROM bigrams WHERE current = ?");
    if (statement) {
      statement->bindTextToColumn(phrase, 1);
      if (statement->step() == SQLITE_ROW) {
        occurances = (size_t)statement->intOfColumn(0);
        while (statement->step() == SQLITE_ROW)
          ;
      }
      delete statement;
    }

    statement = m_connection->prepare(
        "SELECT COUNT(*) FROM bigrams WHERE previous = ?");
    if (statement) {
      statement->bindTextToColumn(phrase, 1);
      if (statement->step() == SQLITE_ROW) {
        occurances += (size_t)statement->intOfColumn(0);
        while (statement->step() == SQLITE_ROW)
          ;
      }
      delete statement;
    }

    return occurances;
  }

  size_t countBigramOccurancesWithBPMF(const string& phrase,
                                       const string& bpmf) {
    size_t occurances = 0;

    string qstring = BPMFUserPhraseHelper::QString(bpmf).first;
    string qs = string("%") + qstring;

    OVSQLiteStatement* statement;
    statement = m_connection->prepare(
        "SELECT COUNT(*) FROM bigrams WHERE current = ? and qstring like ?");
    if (statement) {
      statement->bindTextToColumn(phrase, 1);
      statement->bindTextToColumn(qs, 2);
      if (statement->step() == SQLITE_ROW) {
        occurances = (size_t)statement->intOfColumn(0);
        while (statement->step() == SQLITE_ROW)
          ;
      }
      delete statement;
    }

    qs = qstring + string("%");
    statement = m_connection->prepare(
        "SELECT COUNT(*) FROM bigrams WHERE previous = ? and qstring like ?");
    if (statement) {
      statement->bindTextToColumn(phrase, 1);
      statement->bindTextToColumn(qs, 2);
      if (statement->step() == SQLITE_ROW) {
        occurances += (size_t)statement->intOfColumn(0);
        while (statement->step() == SQLITE_ROW)
          ;
      }
      delete statement;
    }

    return occurances;
  }

  void removePhrase(const string& phrase) {
    m_connection->execute("DELETE FROM bigrams WHERE current = %Q",
                          phrase.c_str());
    m_connection->execute("DELETE FROM bigrams WHERE previous = %Q",
                          phrase.c_str());
    m_connection->execute("DELETE FROM unigrams WHERE current = %Q",
                          phrase.c_str());
  }

  void removePhraseWithBPMF(const string& phrase, const string& bpmf) {
    string qstring = BPMFUserPhraseHelper::QString(bpmf).first;
    m_connection->execute(
        "DELETE FROM unigrams WHERE current = %Q AND qstring = %Q",
        phrase.c_str(), qstring.c_str());

    string qs = string("%") + qstring;
    m_connection->execute(
        "DELETE FROM bigrams WHERE current = %Q AND qstring LIKE %Q",
        phrase.c_str(), qs.c_str());

    qs = qstring + string("%");
    m_connection->execute(
        "DELETE FROM bigrams WHERE previous = %Q AND qstring LIKE %Q",
        phrase.c_str(), qs.c_str());
  }

  void addPhraseWithBPMF(const string& phrase, const string& bpmf) {
    // CREATE TABLE unigrams (qstring, current, probability, backoff);
    m_connection->execute("INSERT INTO unigrams VALUES(%Q, %Q, %f, 0.0)",
                          BPMFUserPhraseHelper::QString(bpmf).first.c_str(),
                          phrase.c_str(), m_maxUnigramProbability);
  }

  void addPhraseWithQueryString(const string& phrase, const string& qstring) {
    // CREATE TABLE unigrams (qstring, current, probability, backoff);
    m_connection->execute("INSERT INTO unigrams VALUES(%Q, %Q, %f, 0.0)",
                          qstring.c_str(), phrase.c_str(),
                          m_maxUnigramProbability);
  }

  void promotePhraseToHighestProbability(const string& phrase) {
    m_connection->execute(
        "UPDATE unigrams SET probability = %f WHERE current = %Q",
        m_maxUnigramProbability, phrase.c_str());
  }

  void demotePhraseToLowestProbability(const string& phrase) {
    m_connection->execute(
        "UPDATE unigrams SET probability = %f WHERE current = %Q",
        m_minUnigramProbability, phrase.c_str());
  }

  void ensurePhraseOrder(const vector<string>& phrases) {
    OVSQLiteStatement* find = m_connection->prepare(
        "SELECT probability FROM unigrams WHERE current = ?");
    vector<double> scores;
    if (find) {
      for (vector<string>::const_iterator pi = phrases.begin();
           pi != phrases.end(); ++pi) {
        find->reset();
        find->bindTextToColumn(*pi, 1);
        if (find->step() == SQLITE_ROW) {
          scores.push_back(find->doubleOfColumn(0));
          while (find->step() == SQLITE_ROW)
            ;
        } else {
          scores.push_back(m_minUnigramProbability);
        }
      }

      size_t c = phrases.size();
      ;
      cout << "old seq: ";

      for (size_t i = 0; i < c; i++) {
        cout << phrases[i] << " (" << scores[i] << ") -- ";
      }
      cout << endl;

      sort(scores.begin(), scores.end());
      reverse(scores.begin(), scores.end());

      cout << "new seq: ";

      for (size_t i = 0; i < c; i++) {
        cout << phrases[i] << " (" << scores[i] << ") -- ";
        m_connection->execute(
            "UPDATE unigrams SET probability = %f WHERE current = %Q",
            scores[i], phrases[i].c_str());
      }
      cout << endl;

      delete find;
    }
  }

  void showUnigramInfo(const vector<string>& phrases) {
    OVSQLiteStatement* find = m_connection->prepare(
        "SELECT qstring, probability FROM unigrams WHERE current = ?");

    for (vector<string>::const_iterator pi = phrases.begin();
         pi != phrases.end(); ++pi) {
      find->reset();
      find->bindTextToColumn(*pi, 1);

      while (find->step() == SQLITE_ROW) {
        cout << "Unigram: " << *pi
             << ", probability: " << find->doubleOfColumn(1);
        cout << ", raw query string: " << find->textOfColumn(0) << ", bpmf: "
             << BPMFUserPhraseHelper::BPMFString(find->textOfColumn(0));
        cout << endl;
      }
    }

    if (find) {
      delete find;
    }
  }

  //
  //     bool addPhrase(const string& phrase);
  //     bool addPhraseWithBopomofoReading(const string& phrase, const string&
  //     bopomofo); bool addPhraseWithQueryString(const string& phrase, const
  //     string& qstring); bool removePhase(const string& phrase);
  //
 protected:
  double findHigestScoreForQueryString(const string& qstring) {
    double result = m_UNKProbability;
    OVSQLiteStatement* statement = m_connection->prepare(
        "SELECT max(probability) FROM unigrams WHERE qstring = ?");
    if (!statement) return m_UNKProbability;

    while (statement->step() == SQLITE_ROW) {
      result = statement->doubleOfColumn(0);
    }

    delete statement;
    return result;
  }

  OVSQLiteConnection* m_connection;
  bool m_ownsConnection;

  double m_UNKProbability;
  double m_maxUnigramProbability;
  double m_minUnigramProbability;
};
};  // namespace Manjusri

#endif
