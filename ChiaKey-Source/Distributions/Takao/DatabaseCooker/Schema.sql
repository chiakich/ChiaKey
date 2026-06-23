PRAGMA page_size=8192;

CREATE TABLE 'Generic-cj-cin' (key, value);
CREATE TABLE 'Generic-simplex-cin' (key, value);
CREATE TABLE 'Punctuations-cj-halfwidth-cin' (key, value);
CREATE TABLE 'Punctuations-cj-mixedwidth-cin' (key, value);
CREATE TABLE 'Mandarin-bpmf-cin' (key, value);
CREATE TABLE 'BopomofoCorrection-bopomofo-correction-cin' (key, value);
CREATE TABLE 'prepopulated_service_data' (key, value);

CREATE INDEX 'Generic-cj-cin-index-on-key' ON 'Generic-cj-cin' (key);
CREATE INDEX 'Generic-cj-cin-index-on-value' ON 'Generic-cj-cin' (value);
CREATE INDEX 'Generic-simplex-cin-index' ON 'Generic-simplex-cin' (key);
CREATE INDEX 'Punctuations-cj-halfwidth-cin-index' ON 'Punctuations-cj-halfwidth-cin' (key);
CREATE INDEX 'Punctuations-cj-mixedwidth-cin-index' ON 'Punctuations-cj-mixedwidth-cin' (key);
CREATE INDEX 'Mandarin-bpmf-cin-index-on-key' ON 'Mandarin-bpmf-cin' (key);
CREATE INDEX 'Mandarin-bpmf-cin-index-on-value' ON 'Mandarin-bpmf-cin' (value);
CREATE INDEX 'BopomofoCorrection-bopomofo-correction-cin-index' ON 'BopomofoCorrection-bopomofo-correction-cin' (key);
CREATE INDEX 'prepopulated_service_data_index' ON 'prepopulated_service_data' (key);
