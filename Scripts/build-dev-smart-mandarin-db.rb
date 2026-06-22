#!/usr/bin/env ruby
# frozen_string_literal: true

require "digest"
require "fileutils"
require "open3"
require "set"

ROOT = File.expand_path("..", __dir__)
SOURCE_CIN = File.join(ROOT, "YahooKeyKey-Source-1.1.2528", "DataTables", "bpmf.cin")
DATA_SOURCE = File.join(ROOT, "YahooKeyKey-Source-1.1.2528", "Distributions", "Takao", "DataSource")
ADDENDUM_DIR = File.join(DATA_SOURCE, "Addendum")
OVERRIDES_DIR = File.join(DATA_SOURCE, "Overrides")
MODERN_DIR = File.join(DATA_SOURCE, "Modern")
OUTPUT_DB = File.join(
  ROOT,
  "YahooKeyKey-Source-1.1.2528",
  "Distributions",
  "Takao",
  "CookedDatabase",
  "KeyKeySource.db"
)

VERSION = "chiaki-modern-2026.06-dev1"
MAX_PHRASE_CODEPOINTS = 7
PROB_UNK = -99.0
PROB_SINGLE_PRIMARY = -3.0
PROB_SINGLE_SECONDARY = -9.0
PROB_PHRASE_ADDENDUM = -1.35
PROB_PHRASE_MODERN = -0.85
PROB_OVERRIDE_ADD = -0.6
PROB_EXPLICIT_BPMF = -0.5
PROB_PROMOTE_HIGHEST = -0.05
PROB_DEMOTE_LOWEST = -40.0
PROB_BIGRAM = -0.1

COMPONENTS = {
  "ㄅ" => 0x0001, "ㄆ" => 0x0002, "ㄇ" => 0x0003, "ㄈ" => 0x0004,
  "ㄉ" => 0x0005, "ㄊ" => 0x0006, "ㄋ" => 0x0007, "ㄌ" => 0x0008,
  "ㄍ" => 0x0009, "ㄎ" => 0x000a, "ㄏ" => 0x000b, "ㄐ" => 0x000c,
  "ㄑ" => 0x000d, "ㄒ" => 0x000e, "ㄓ" => 0x000f, "ㄔ" => 0x0010,
  "ㄕ" => 0x0011, "ㄖ" => 0x0012, "ㄗ" => 0x0013, "ㄘ" => 0x0014,
  "ㄙ" => 0x0015, "ㄧ" => 0x0020, "ㄨ" => 0x0040, "ㄩ" => 0x0060,
  "ㄚ" => 0x0080, "ㄛ" => 0x0100, "ㄜ" => 0x0180, "ㄝ" => 0x0200,
  "ㄞ" => 0x0280, "ㄟ" => 0x0300, "ㄠ" => 0x0380, "ㄡ" => 0x0400,
  "ㄢ" => 0x0480, "ㄣ" => 0x0500, "ㄤ" => 0x0580, "ㄥ" => 0x0600,
  "ㄦ" => 0x0680, "ˊ" => 0x0800, "ˇ" => 0x1000, "ˋ" => 0x1800,
  "˙" => 0x2000
}.freeze

BPMF_COMPONENTS = COMPONENTS.keys.freeze

SourceStats = Struct.new(:kind, :path, :sha256, :seen, :added, :skipped, keyword_init: true)

def sql(value)
  "'#{value.to_s.gsub("'", "''")}'"
end

def relative_path(path)
  path.delete_prefix(ROOT + "/")
end

def source_stats_for(stats, path, kind)
  stats[path] ||= SourceStats.new(
    kind: kind,
    path: relative_path(path),
    sha256: File.file?(path) ? Digest::SHA256.file(path).hexdigest : "",
    seen: 0,
    added: 0,
    skipped: 0
  )
end

def absolute_order_string(components)
  syllable = 0
  components.each { |component| syllable |= COMPONENTS.fetch(component) }
  order = (syllable & 0x001f) +
          (((syllable & 0x0060) >> 5) * 22) +
          (((syllable & 0x0780) >> 7) * 22 * 4) +
          (((syllable & 0x3800) >> 11) * 22 * 4 * 14)

  (48 + (order % 79)).chr + (48 + (order / 79)).chr
end

def parse_cin(path)
  keynames = {}
  chardefs = []
  properties = []

  lines = File.readlines(path, chomp: true)
  index = 0

  while index < lines.length
    line = lines[index]

    if line =~ /^%keyname\s+begin/i
      index += 1
      until index >= lines.length || lines[index] =~ /^%keyname\s+end/i
        key, value = lines[index].split(/\s+/, 2)
        keynames[key] = value if key && value
        index += 1
      end
    elsif line =~ /^%chardef\s+begin/i
      index += 1
      until index >= lines.length || lines[index] =~ /^%chardef\s+end/i
        key, value = lines[index].split(/\s+/, 2)
        chardefs << [key, value] if key && value
        index += 1
      end
    elsif line.start_with?("%") && line !~ /^%(gen_inp|keyname|chardef)/i
      key, value = line[1..-1].split(/\s+/, 2)
      properties << [key, value] if key && value
    end

    index += 1
  end

  [keynames, chardefs, properties]
end

def qstring_for_key(key, keynames)
  components = key.each_char.map { |char| keynames[char] }.compact
  return nil if components.empty?

  absolute_order_string(components)
rescue KeyError
  nil
end

def qstring_for_bpmf_syllable(syllable)
  components = syllable.each_char.select { |char| COMPONENTS.key?(char) }
  return nil if components.empty?

  absolute_order_string(components)
rescue KeyError
  nil
end

def qstring_for_bpmf_sequence(sequence)
  qstrings = sequence.split(",").map { |syllable| qstring_for_bpmf_syllable(syllable.strip) }
  return nil if qstrings.empty? || qstrings.any?(&:nil?)

  qstrings.join
end

def bopomofo_candidate?(text)
  text.each_char.any? { |char| BPMF_COMPONENTS.include?(char) }
end

def phrase_candidate?(text)
  return false if text.empty? || bopomofo_candidate?(text)
  return false if text.each_char.count > MAX_PHRASE_CODEPOINTS
  return false if text =~ %r{https?://}

  true
end

def strip_inline_comment(line)
  line.sub(/\s+#.*$/, "").strip
end

def each_data_line(path)
  return enum_for(:each_data_line, path) unless block_given?

  File.foreach(path, chomp: true) do |line|
    line = strip_inline_comment(line)
    next if line.empty? || line.start_with?("#")

    yield line
  end
end

def infer_qstring_for_text(text, readings_by_text)
  qstrings = []

  text.each_char do |char|
    readings = readings_by_text[char]
    return nil unless readings && !readings.empty?

    qstrings << readings.first
  end

  qstrings.join
end

def add_table_row(table_rows, table_row_set, qstring, text)
  key = [qstring, text]
  return false if table_row_set.include?(key)

  table_row_set.add(key)
  table_rows << key
  true
end

def add_unigram(unigrams, qstring, text, probability, backoff = 0.0)
  key = [qstring, text]
  existing = unigrams[key]
  return false if existing && existing[:probability] >= probability

  unigrams[key] = { probability: probability, backoff: backoff }
  true
end

def remove_unigram(unigrams, qstring, text)
  unigrams.delete([qstring, text])
end

def add_phrase(text, probability, source_path, kind, readings_by_text, unigrams, table_rows, table_row_set, stats)
  stat = source_stats_for(stats, source_path, kind)
  stat.seen += 1
  return stat.skipped += 1 unless phrase_candidate?(text)

  qstring = infer_qstring_for_text(text, readings_by_text)
  return stat.skipped += 1 unless qstring

  added = add_unigram(unigrams, qstring, text, probability)
  add_table_row(table_rows, table_row_set, qstring, text)
  added ? stat.added += 1 : stat.skipped += 1
end

def add_explicit_bpmf(text, bpmf, probability, source_path, kind, unigrams, table_rows, table_row_set, stats)
  stat = source_stats_for(stats, source_path, kind)
  stat.seen += 1
  return stat.skipped += 1 unless phrase_candidate?(text)

  qstring = qstring_for_bpmf_sequence(bpmf)
  return stat.skipped += 1 unless qstring

  added = add_unigram(unigrams, qstring, text, probability)
  add_table_row(table_rows, table_row_set, qstring, text)
  added ? stat.added += 1 : stat.skipped += 1
end

unless File.exist?(SOURCE_CIN)
  warn "Missing source CIN: #{SOURCE_CIN}"
  exit 1
end

FileUtils.mkdir_p(File.dirname(OUTPUT_DB))
FileUtils.rm_f(OUTPUT_DB)

stats = {}
unigrams = {}
bigrams = {}
table_rows = []
table_row_set = Set.new
readings_by_text = Hash.new { |hash, key| hash[key] = [] }

keynames, chardefs, properties = parse_cin(SOURCE_CIN)
cin_stat = source_stats_for(stats, SOURCE_CIN, "cin")

chardefs.each do |key, value|
  cin_stat.seen += 1
  qstring = qstring_for_key(key, keynames)
  if qstring
    add_table_row(table_rows, table_row_set, qstring, value)

    if phrase_candidate?(value)
      readings_by_text[value] << qstring if value.each_char.count == 1 && !readings_by_text[value].include?(qstring)
      probability = readings_by_text[value].first == qstring ? PROB_SINGLE_PRIMARY : PROB_SINGLE_SECONDARY
      add_unigram(unigrams, qstring, value, probability)
      cin_stat.added += 1
    else
      cin_stat.skipped += 1
    end
  else
    cin_stat.skipped += 1
  end
end

unigrams[["*", ""]] = { probability: PROB_UNK, backoff: 0.0 }
unigrams[["!", ""]] = { probability: 0.0, backoff: 0.0 }
unigrams[["$", ""]] = { probability: 0.0, backoff: 0.0 }

Dir[File.join(ADDENDUM_DIR, "*.txt")].sort.each do |path|
  each_data_line(path) do |phrase|
    add_phrase(phrase, PROB_PHRASE_ADDENDUM, path, "addendum", readings_by_text, unigrams, table_rows, table_row_set, stats)
  end
end

Dir[File.join(MODERN_DIR, "*.txt")].sort.each do |path|
  each_data_line(path) do |phrase|
    add_phrase(phrase, PROB_PHRASE_MODERN, path, "modern", readings_by_text, unigrams, table_rows, table_row_set, stats)
  end
end

Dir[File.join(OVERRIDES_DIR, "*.txt")].sort.each do |path|
  stat = source_stats_for(stats, path, "override")

  each_data_line(path) do |line|
    parts = line.split(/\s+/)
    command = parts.shift
    added = false

    case command
    when "+"
      phrase = parts.join("")
      add_phrase(phrase, PROB_OVERRIDE_ADD, path, "override", readings_by_text, unigrams, table_rows, table_row_set, stats)
      next
    when "-"
      stat.seen += 1
      phrase = parts.join("")
      if phrase_candidate?(phrase)
        qstring = infer_qstring_for_text(phrase, readings_by_text)
        added = !!(qstring && remove_unigram(unigrams, qstring, phrase))
      end
    when "+bpmf"
      phrase = parts.shift.to_s
      bpmf = parts.join
      add_explicit_bpmf(phrase, bpmf, PROB_EXPLICIT_BPMF, path, "override", unigrams, table_rows, table_row_set, stats)
      next
    when "-bpmf"
      stat.seen += 1
      phrase = parts.shift.to_s
      bpmf = parts.join
      qstring = qstring_for_bpmf_sequence(bpmf)
      added = !!(qstring && remove_unigram(unigrams, qstring, phrase))
    when "+2"
      stat.seen += 1
      previous = parts.shift.to_s
      current = parts.join
      previous_qstring = infer_qstring_for_text(previous, readings_by_text)
      current_qstring = infer_qstring_for_text(current, readings_by_text)
      if previous_qstring && current_qstring
        bigrams[[previous_qstring + " " + current_qstring, previous, current]] = PROB_BIGRAM
        added = true
      end
    when "promote-highest"
      stat.seen += 1
      phrase = parts.join("")
      if phrase_candidate?(phrase)
        qstring = infer_qstring_for_text(phrase, readings_by_text)
        if qstring
          add_unigram(unigrams, qstring, phrase, PROB_PROMOTE_HIGHEST)
          add_table_row(table_rows, table_row_set, qstring, phrase)
          added = true
        end
      end
    when "demote-lowest"
      stat.seen += 1
      phrase = parts.join("")
      if phrase_candidate?(phrase)
        qstring = infer_qstring_for_text(phrase, readings_by_text)
        if qstring
          add_unigram(unigrams, qstring, phrase, PROB_DEMOTE_LOWEST)
          add_table_row(table_rows, table_row_set, qstring, phrase)
          added = true
        end
      end
    when "ensure-order"
      stat.seen += 1
      preferred = parts.shift.to_s
      demoted = parts.shift.to_s
      preferred_qstring = infer_qstring_for_text(preferred, readings_by_text)
      demoted_qstring = infer_qstring_for_text(demoted, readings_by_text)
      if preferred_qstring && demoted_qstring && preferred_qstring == demoted_qstring
        add_unigram(unigrams, preferred_qstring, preferred, PROB_PROMOTE_HIGHEST)
        add_unigram(unigrams, demoted_qstring, demoted, PROB_DEMOTE_LOWEST)
        add_table_row(table_rows, table_row_set, preferred_qstring, preferred)
        add_table_row(table_rows, table_row_set, demoted_qstring, demoted)
        added = true
      end
    else
      stat.seen += 1
    end

    added ? stat.added += 1 : stat.skipped += 1
  end
end

sql_lines = []
sql_lines << "PRAGMA page_size=8192;"
sql_lines << "CREATE TABLE cooked_information (key, value);"
sql_lines << "CREATE TABLE prepopulated_service_data (key, value);"
sql_lines << "CREATE TABLE chiaki_db_metadata (key, value);"
sql_lines << "CREATE TABLE chiaki_db_sources (source, kind, sha256, seen, added, skipped);"
sql_lines << "CREATE TABLE unigrams (qstring, current, probability, backoff);"
sql_lines << "CREATE TABLE bigrams (qstring, previous, current, probability);"
sql_lines << "CREATE TABLE 'Mandarin-bpmf-cin' (key, value);"
sql_lines << "CREATE INDEX unigrams_index ON unigrams (qstring);"
sql_lines << "CREATE INDEX unigrams_current_index ON unigrams(current);"
sql_lines << "CREATE INDEX bigrams_index ON bigrams (qstring);"
sql_lines << "CREATE INDEX 'Mandarin-bpmf-cin-index-on-key' ON 'Mandarin-bpmf-cin' (key);"
sql_lines << "CREATE INDEX 'Mandarin-bpmf-cin-index-on-value' ON 'Mandarin-bpmf-cin' (value);"
sql_lines << "CREATE INDEX chiaki_db_metadata_index ON chiaki_db_metadata (key);"
sql_lines << "BEGIN;"
sql_lines << "INSERT INTO cooked_information VALUES('version', #{sql(VERSION)});"
sql_lines << "INSERT INTO chiaki_db_metadata VALUES('schema_version', '1');"
sql_lines << "INSERT INTO chiaki_db_metadata VALUES('version', #{sql(VERSION)});"
sql_lines << "INSERT INTO chiaki_db_metadata VALUES('generator', 'Scripts/build-dev-smart-mandarin-db.rb');"
sql_lines << "INSERT INTO chiaki_db_metadata VALUES('max_phrase_codepoints', #{sql(MAX_PHRASE_CODEPOINTS)});"
sql_lines << "INSERT INTO chiaki_db_metadata VALUES('unigram_count', #{sql(unigrams.size)});"
sql_lines << "INSERT INTO chiaki_db_metadata VALUES('bigram_count', #{sql(bigrams.size)});"
sql_lines << "INSERT INTO chiaki_db_metadata VALUES('candidate_count', #{sql(table_rows.size)});"

stats.values.sort_by(&:path).each do |stat|
  sql_lines << "INSERT INTO chiaki_db_sources VALUES(#{sql(stat.path)}, #{sql(stat.kind)}, #{sql(stat.sha256)}, #{stat.seen}, #{stat.added}, #{stat.skipped});"
end

properties.each do |key, value|
  sql_lines << "INSERT INTO 'Mandarin-bpmf-cin' VALUES(#{sql("__property_#{key}")}, #{sql(value)});"
end

keynames.each do |key, value|
  sql_lines << "INSERT INTO 'Mandarin-bpmf-cin' VALUES(#{sql("__property_keyname-#{key}")}, #{sql(value)});"
end

table_rows.sort.each do |key, value|
  sql_lines << "INSERT INTO 'Mandarin-bpmf-cin' VALUES(#{sql(key)}, #{sql(value)});"
end

unigrams.sort_by { |(key, value), gram| [key, value, -gram[:probability]] }.each do |(key, value), gram|
  sql_lines << "INSERT INTO unigrams VALUES(#{sql(key)}, #{sql(value)}, #{gram[:probability]}, #{gram[:backoff]});"
end

bigrams.sort.each do |(qstring, previous, current), probability|
  sql_lines << "INSERT INTO bigrams VALUES(#{sql(qstring)}, #{sql(previous)}, #{sql(current)}, #{probability});"
end

sql_lines << "COMMIT;"

stdout, stderr, status = Open3.capture3("/usr/bin/sqlite3", OUTPUT_DB, stdin_data: sql_lines.join("\n"))
unless status.success?
  warn stderr
  warn stdout
  exit status.exitstatus || 1
end

puts "Wrote #{OUTPUT_DB}"
puts "Version: #{VERSION}"
puts "Mandarin-bpmf-cin rows: #{table_rows.size}"
puts "Unigram rows: #{unigrams.size}"
puts "Bigram rows: #{bigrams.size}"
puts "Sources:"
stats.values.sort_by(&:path).each do |stat|
  puts "  #{stat.kind}: #{stat.path} seen=#{stat.seen} added=#{stat.added} skipped=#{stat.skipped}"
end
