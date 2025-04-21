# CAN Log Analyzer (`can_analyzer.py`)

This Python script (`can_analyzer.py`) analyzes CAN bus and serial communication log files, **automatically detects the log format**, extracts unique CAN messages or serial lines, and optionally **filters the output by one or more CAN IDs or the "SERIAL" type**. It provides a concise summary of the results and can output a condensed version of the log.

## Features

*   **Format Auto-Detection:** Automatically detects the input log format based on the header or first line.
*   **Parses Multiple Log Formats:** Supports:
    *   **Custom CSV:** `Type,Timestamp,Identifier,DLC,D1...` or `Type,Timestamp,Data...`
    *   **SavvyCAN CSV:** `Time Stamp,ID,Extended,Dir,Bus,LEN,D1...`
    *   **`candump -l` style:** `(timestamp) interface ID#DATA`
*   **Identifies Unique Payloads:** Determines unique messages based on the combination of the identifier (CAN ID or "SERIAL") and the data payload.
*   **Counts Message Occurrences:** Counts how many times each unique identifier/payload combination appears in the log (after filtering).
*   **Filters by Multiple Identifiers:** Optionally filters the output to show only messages matching **one or more** specified CAN IDs (hexadecimal) or the type "SERIAL".
*   **Outputs Condensed Log or Summary:**
    *   Can output a **condensed log** (to file or console) containing only the *first occurrence* of each unique payload for a given *filtered* identifier, preserving original timestamps and line formats.
    *   Always prints a **summary** to the console (stderr) showing processing statistics and a list of all unique payloads found for the *filtered* identifiers, along with their total counts.
*   **Clear Summary:** Provides a summary of total lines processed, messages processed after filtering, and unique payloads found for the filtered set.
*   **Error Handling:** Includes basic error handling for file operations and parsing.
*   **Command-Line Arguments:** Uses `argparse` for easy and flexible command-line usage.

## Why Use This Tool?

*   **Reduce Log Size:** Significantly shrinks log files by removing repetitive status updates when outputting the *condensed log*.
*   **Focus on Changes:** Makes it easier to identify when the data payload for a specific CAN ID or the Serial line actually changes.
*   **Targeted Analysis:** Filter for specific CAN IDs or Serial data to isolate relevant traffic.
*   **Improve Readability:** The *condensed log* is cleaner for manual inspection or comparison (diffing). The *summary output* provides a quick overview of all distinct messages seen for the filtered identifiers.
*   **Faster Analysis:** Smaller condensed logs can be processed more quickly by other tools. The summary gives immediate insight into message variety within the filtered set.

## How it Works (Filtering Logic)

The script reads the input log line by line and detects the format. It maintains a record (dictionary) of the *last data payload seen for each unique identifier*. An identifier is either:
    *   The **CAN Arbitration ID** (hex converted to int).
    *   The literal string `"SERIAL"` (for the custom CSV format).

For each new line read:
1.  It extracts the `Identifier` (CAN ID or "SERIAL").
2.  **Filtering:** If filter(s) are provided via the `-f` argument, it checks if the current `Identifier` matches any of the specified filters. If it doesn't match, the line is skipped entirely.
3.  **Uniqueness Check (for Condensed Output):** If the line passes the filter (or if no filter is applied), it extracts the relevant `Data` payload portion into a consistent tuple format. It compares this `current Data tuple` to the `last seen Data tuple` stored for that specific `Identifier`.
4.  **Condensed Output:**
    *   **If** the `current Data tuple` is **different** from the `last seen Data tuple` for that `Identifier` (or if it's the first time seeing that `Identifier`), the current **original line** is written to the condensed output (file or stdout) and the `last seen Data tuple` is updated.
    *   **If** the `current Data tuple` is **identical** to the `last seen Data tuple` for that `Identifier`, the current line is **skipped** for the condensed output.
5.  **Summary Statistics:** Regardless of whether the line was written to the condensed output (due to uniqueness), if it passed the filter, the script updates the count for its specific payload in the summary statistics dictionary.

## Requirements

*   **Python 3** (3.6 or later recommended)
*   No external libraries required beyond standard Python modules (`csv`, `argparse`, `sys`, `os`, `re`).

## Installation

No installation is typically needed. Just ensure you have Python 3 installed. You might want to make the script executable:

```bash
chmod +x can_analyzer.py
```

## Usage

```bash
python can_analyzer.py <log_file> [-o <output_file>] [-f <identifier1> [<identifier2> ...]]
# or if executable:
./can_analyzer.py <log_file> [-o <output_file>] [-f <identifier1> [<identifier2> ...]]
```

*   **`<log_file>`:** (Required) Path to the input log file (script will attempt auto-detection).
*   **`-o <output_file>` or `--output <output_file>`:** (Optional) Path to the output file for the **condensed log**. If omitted, the condensed log is printed to standard output (console). The summary is *always* printed to standard error (console).
*   **`-f <identifier1> [<identifier2> ...]` or `--filter <identifier1> [<identifier2> ...]`:** (Optional) **One or more** identifiers to filter for. Each identifier can be a CAN ID (in hexadecimal, e.g., `0x1A1`) or the literal string `"SERIAL"` (case-insensitive). If specified, *only* messages matching *any* of these identifiers will be processed. If omitted, all messages are processed.
*   **`-h` or `--help`:** Show the help message.

**Examples:**

1.  **Analyze all messages from `log.csv`, print condensed log and summary to console:**
    ```bash
    ./can_analyzer.py log.csv
    ```

2.  **Analyze all messages from `log.csv`, save condensed log to `condensed.csv`, print summary to console:**
    ```bash
    ./can_analyzer.py log.csv -o condensed.csv
    ```

3.  **Analyze *only* messages with ID `0x161` from `savvy.csv`, save condensed log to `condensed_161.csv`, print summary to console:**
    ```bash
    ./can_analyzer.py savvy.csv -f 0x161 -o condensed_161.csv
    ```
   or
    ```bash
    ./can_analyzer.py savvy.csv --filter 0x161 --output condensed_161.csv
    ```
4. **Analyze *only* SERIAL messages from `log.csv`, print condensed log and summary to console:**
    ```bash
    ./can_analyzer.py log.csv -f SERIAL
    ```
5. **Analyze messages with IDs `0x1A1`, `0xDF`, and `SERIAL` from `log.csv`, print condensed log and summary to console:**
    ```bash
    ./can_analyzer.py log.csv -f 0x1A1 0xDF SERIAL
    ```
6. **Analyze the list of 'unknown' CAN IDs, save condensed log, print summary:**
    ```bash
    ./can_analyzer.py your_log.csv -o unknown_condensed.log -f \
    0x9F 0xA4 0xDF 0x11F 0x120 0x12D 0x15B 0x18C 0x1A8 0x1CC 0x1DF 0x1E1 \
    0x217 0x227 0x24C 0x257 0x2A0 0x2A5 0x2E1 0x317 0x361 0x3A7 0x3F6 \
    0x412 0x49F 0x4A0 0x512 0x51F 0x520 0x525 0x531 0x5D2 0x5DD 0x5DF 0x5F1
    ```

## Supported Input Log Formats

(Same as previous version - Included for completeness)

The script attempts to automatically detect one of the following formats:

1.  **Custom CSV:**
    ```csv
    Type,Timestamp,Identifier,DLC,D1,D2,D3,D4,D5,D6,D7,D8
    CAN,1743874896.997287,161,8,00,00,A1,30,FF,FF,FF,FF
    SERIAL,1743874896.998507,FD 0B 56 00 0C 0E 09 09 05 00 0F A1 FD 0B
    ```
    *(Note: Fewer data columns are acceptable)*

2.  **SavvyCAN CSV:**
    ```csv
    Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8
    39966311,00000167,false,Rx,0,8,09,06,FF,FF,00,00,00,00
    39989355,0000024C,false,Rx,0,5,00,00,00,00,00,,,
    ```

3.  **`candump -l` style:**
    ```
    (1678886400.100000) vcan0 123#DEADC0DE
    (1678886400.200000) vcan0 456#AABB
    (1678886400.300000) vcan0 789#
    ```

## Output Formats

The script produces two types of output:

1.  **Condensed Log Output (Optional: File/Stdout):**
    *   If the `-o` option is used, this output is saved to the specified file.
    *   If `-o` is *not* used, this output is printed to the standard output (console).
    *   This output contains the **original lines** from the input log file that were *kept* (i.e., lines matching the filter *and* representing the first occurrence of a unique payload for a given identifier, or subsequent lines where the payload changed for that identifier). The original format (CSV or candump line) is preserved.

2.  **Summary Output (Always: Stderr):**
    *   This summary is *always* printed to the standard error (console) at the end of the script's execution.
    *   It includes:
        *   Total lines processed from the input file.
        *   Total messages processed *after* applying any `--filter`.
        *   Total unique message payloads found (for the filtered identifiers).
        *   A detailed list of each unique payload found for each identifier within the filtered set, sorted by identifier, then by payload:
            *   **Identifier:** The CAN ID (e.g., `0x161`) or `"SERIAL"`.
            *   **DLC:** The Data Length Code (for CAN messages).
            *   **Data:** The data payload bytes (hexadecimal, space-separated for CAN) or the serial data content.
            *   **Count:** The total number of times that specific payload occurred for that identifier in the processed (filtered) messages.

    **Example Summary Output (when filtering for 0x1A1 and SERIAL):**
    ```
    --- Analysis Summary ---
    Total lines processed: 1589
    Total messages processed after filter: 53
    Total unique message payloads found (after filter): 12

    Unique Messages Summary:
      Identifier: 0x1A1
        DLC: 8, Data: FF 00 00 00 00 00 00 00, Count: 1
      Identifier: SERIAL
        Data: [FD 08 32 02 00 00 00 00 00 00 3C], Count: 10
        Data: [FD 03 29 00 00 2C], Count: 10
        Data: [FD 07 38 00 08 00 05 00 00 4C], Count: 8
        # ... other unique SERIAL messages ...
    ```

## Limitations and Notes

*   **Format Detection:** Auto-detection relies on common header patterns or the first line structure. It might fail on heavily modified or unusual log files.
*   **Consecutive Duplicates Only:** Only removes duplicates that occur *immediately* after the previous identical message *from the same identifier* when generating the condensed output. Non-consecutive identical messages are kept. The summary counts *all* occurrences after filtering.
*   **No Protocol Interpretation:** Does not understand the meaning of CAN IDs or serial data payloads. Filtering is purely based on identical raw data strings/bytes for the payload comparison.
