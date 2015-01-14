const char *medialib_sql =
"CREATE TABLE track \
( \
	id integer primary key, \
	file varchar(255), \
	length integer, \
	artist varchar(255), \
	title varchar(255), \
	album varchar(255), \
	label varchar(64), \
	comment varchar(128), \
	rating_explicit integer, \
	rating_implicit integer, \
	date timestamp, \
	genre varchar(64), \
	tempo integer, \
	license varchar(32), \
	type integer, \
	play_count integer, \
	skip_count integer, \
	file_missing integer \
); \
\
CREATE TABLE aditional_trackinfo \
( \
	track_id integer, \
	key varchar(32), \
	value varchar(255) \
); \
\
CREATE TABLE similarity \
( \
	track_id_a integer, \
	track_id_b integer, \
	similarity integer \
); \
\
CREATE TABLE path \
( \
	id integer primary key, \
	path varchar(255), \
	date timestamp \
);";
