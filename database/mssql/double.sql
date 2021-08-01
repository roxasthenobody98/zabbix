ALTER TABLE trends
	ALTER COLUMN value_min DOUBLE PRECISION,
	ADD DEFAULT ('0.0000') FOR value_min,
	ALTER COLUMN value_avg DOUBLE PRECISION,
	ADD DEFAULT ('0.0000') FOR value_avg,
	ALTER COLUMN value_max DOUBLE PRECISION,
	ADD DEFAULT ('0.0000') FOR value_max;
ALTER TABLE history
	ALTER COLUMN value DOUBLE PRECISION,
	ADD DEFAULT ('0.0000') FOR value;
