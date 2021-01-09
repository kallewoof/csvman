### Labels

CSV files begin with a one line comma-separated header, labeling each column, followed by any number of data rows.

```
Name,Location
Kalle,Japan
```

This can be declared in CMF as such:

```
name = "Name";
location = "Location";
```

### Keys and values

Generally, data is grouped into keys -- which represent a query -- and values -- which represent an answer to the query.

For example, if the question is: what is the location of these people, the name variable above would be the key, and the location would be the value. Variables are by default values.

```
key name = "Name";
location = "Location";
```

(As a sidenote, if the question is "who's at the given location", the location would be the key, and the name would be the value.)

Identifying keys in this manner is necessary when working with multiple data sources (CSV files), in particular when merging data sets.

CMF takes a CSV file as input, converts it into a mapping where the key is the set of keys in the specification (name, in this case) and the value is the set of values (location), and then writes a new CSV file based for each value in this mapping.

In other words, when working with multiple data sets, they must all have the same set of keys (although they can be ordered differently).

### Fitting values

When using data sets from multiple independent sources, very frequently the sources will be classifying things in slightly incompatible ways. For example, in various COVID-19 data sets,
* two data sets had Province/State and Country/Region
* another had countryName and region.

However,
* one of these had Province/State be Anguilla and Country/Region be United Kingdom,
* the other had Country/Region be Anguilla and Province/State be United Kingdom,
* the third had countryName Anguilla and region be Americas.

Needless to say, we want to just group these all into an "Anguilla" key and have them all be equivalent. CMF lets you achieve that (mostly) by fitting values:

Data set 1 above:

```
state = "Province/State";
region = "Country/Region";
key location = fit state, region;
```

Data set 2 above:

```
state = "Country/Region";
region = "Province/State";
key location = fit state, region;
```

Data set 3:

```
state = "countryName";
region = "region";
key location = fit state, region;
```

Whenever location is evaluated, the value of the state is looked at first ("Anguilla"). If this value has been seen before, it is accepted, otherwise the value of the region ("United Kingdom") is looked at. If this has not been seen before either (it hasn't), the system earmarks the first non-empty value (state or region, whichever one is not equal to "", i.e. "Anguilla") and returns that.

Whenever *any* fitting operation encounters the keyword "Anguilla", it will be selected and returned immediately, no questions asked.

In other words, when the above process moves on to merging the second data set above, it will eventually fit the state "Anguilla" and region "United Kingdom", which will return "Anguilla", and the third data set will eventually fit the state "Anguilla" and the region "Americas" and, also, return "Anguilla".

Note that fitted values are not written to the output (similarly to the fact they are not read, either).

### Exceptions

Sometimes one data set will call it Burma while another data set calls it Myanmar. It's very manual right now, but we can tell CMF to swap out one for the other:

```
state = "Country/Region" except {
    "Burma" == "Myanmar",
};
```

Whenever the given data set processes a state that claims to be "Burma", it will simply rewrite the name as "Myanmar".

### Aggregation

Data sets can come with varying granularity. For example, a US data set may be down to each city, whereas an international data set would simply list the sum of all the values for all the cities and states in the US under the "US" entry.

We can tell CMF to aggregate (sum up) values when it encounters multiple entries of the same key set:

```
confirmed = sum("Confirmed");
recovered = sum("Recovered");
deaths = sum("Deaths");
```

### Formatting

Specifically dates can come in various formats, and the format is sometimes a part of the specification. CMF lets you describe the format of a value so that you can convert it as part of the merging process:

```
key date = "Date" as { "%u-%u-%u", year, month, day };
```

The above describes the date key as using the "Date" column, split into components using the given scanf string. The first value is the year, the second one is the month, and the third is the day.

As such, when this interprets the string "2021-01-09", it will extract year=2021, month=01, and day=09.

When the above date is passed on to this:

```
key date2 = "Date" as { "%u/%u/%u", year, month, day };
```

it will come out as "2021/01/09" in the output.

### Trailing data and aspects

Sometimes, data is represented as a 2-dimensional map where the top header's right-hand columns are values as well. For example:

```
Name,2021-01-04,2021-01-05,2021-01-06,2021-01-07,...
Kalle,WFH,WFH,WFH,WFH,...
Lucy,Vacation,Vacation,WFH,WFH,...
Peter,WFH,WFH,Office,WFH,...
```

I.e. a column is added at the start of each day, and the employees fill in where they are that day.
Here, the actual headers are part of the data, i.e. the 'date'. We can represent this in CMF using the '*' value:

```
key name = "Name";
key date = * as { "%u-%u-%u", year, month, day };
aspect location;
```

Since the date *value* is stored in the header itself, we also need to explain what the individual values below it mean. The third line declares the name of the value in the date columns.

In some cases there are multiple files with the same name and date values, but containing different data (e.g. confirmed cases, recovered cases, and deaths, for a pandemic). These three documents define *aspects* of a given data. In CMF, they can be declared as:

```
aspects confirmed, recovered, deaths;
```
