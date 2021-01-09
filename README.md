# csvman
CSV file manipulation tool

This tool primarily lets you merge CSV data from multiple sources and time periods, and was developed as COVID-19 data sources kept going dark.

The tool revolves around the CMF format (CSV Manipulator Formula), which is a simple language for describing the layout of a CSV file. There are several example formulas in the formulas folder.

## Quick start

Compile:

```Bash
g++ -O3 -std=c++11 parser/*.cpp *.cpp -o compile
```

Obtain data:

```Bash
git clone git@github.com:datasets/covid-19.git gds-data
```

Manipulate data:

```Bash
./compile formulas/covid-19/gds.cmf gds-data/time-series-19-covid-combined.csv -f formulas/covid-19/ulklc.cmf result.csv
```

Should now have three files result_confirmed|recovered|deaths.csv in the CSSEGI COVID-19 format.
