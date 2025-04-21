#!/usr/bin/env python3

import argparse
import csv
import sys
import os
import re # Import re for candump regex

def detect_format(first_line):
    """
    Detects the log format based on the first line.

    Args:
        first_line (str): The first line of the log file.

    Returns:
        str: "csv", "savvycan_csv", "candump", or "unknown"
    """
    if not first_line:
        return "unknown" # Empty file

    # Normalize header for comparison
    header_check = first_line.strip().lower().replace(" ", "")

    # --- Check for SavvyCAN CSV format FIRST ---
    # Example: Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2...
    if header_check.startswith("timestamp,id,extended,dir,bus,len,d1"):
        return "savvycan_csv"

    # Check for the custom CSV format
    # Example: Type,Timestamp,Identifier,DLC,D1... OR Type,Timestamp,Data...
    if header_check.startswith("type,timestamp,") or header_check.startswith("type,timestamp,identifier,"):
        return "csv"

    # Check for candump format (starts with '(')
    if first_line.strip().startswith("("):
        return "candump"

    # Add more checks if other formats are common

    return "unknown" # Default if not recognized

def analyze_can_log(log_file, output_file=None, filter_set=None): # Changed id_filter to filter_set
    """
    Analyzes a CAN log file (CSV, SavvyCAN CSV, or candump format),
    extracts unique messages for the filtered set, counts all occurrences
    within the filter, and optionally outputs a condensed log.
    Autodetects format.

    Args:
        log_file (str): Path to the CAN log file.
        output_file (str, optional): Path to the condensed output file. If None, prints condensed log to stdout.
        filter_set (set, optional): Set containing identifiers (int CAN IDs and/or "SERIAL")
                                     to include. If None or empty, all processed.

    Returns:
        None. Prints or writes the unique messages.
    """
    if filter_set is None: # Ensure filter_set is always a set for checks later
        filter_set = set()

    unique_messages = {}  # Dictionary to store unique messages: { identifier: {data_tuple: count} }
    last_data_per_id = {} # Dictionary to store last printed data tuple per identifier (for CONDENSED output)
    total_messages = 0
    lines_processed = 0
    unique_payload_count = 0 # Count of distinct identifier+payload combinations found AFTER filtering
    filtered_messages_count = 0 # Count of messages processed after filtering

    # --- Regex for candump -l format ---
    candump_regex = re.compile(r"^\s*\([\d.]+\)\s+\S+\s+([0-9A-Fa-f]+)(?:#([0-9A-Fa-f]*))?\s*$")

    try:
        with open(log_file, 'r', newline='', encoding='utf-8', errors='replace') as infile:
            # --- Format Detection ---
            first_line = infile.readline()
            if not first_line:
                print(f"Warning: Input file '{log_file}' is empty.")
                return
            file_format = detect_format(first_line)
            infile.seek(0) # Reset file pointer to the beginning

            print(f"Detected format: {file_format}")
            if file_format == "unknown":
                print(f"Error: Could not determine log file format from first line: '{first_line.strip()}'. Exiting.")
                return

            # --- Setup Output ---
            writer = None
            outfile = None
            if output_file:
                try:
                    outfile = open(output_file, 'w', newline='', encoding='utf-8')
                    # Use writer appropriate for the input format if condensing
                    if file_format in ["csv", "savvycan_csv"]:
                        writer = csv.writer(outfile)
                    # For candump, we write raw lines if condensing to a file
                    output_target_name = output_file
                except IOError as e:
                    print(f"Error opening output file '{output_file}': {e}. Outputting condensed log to console instead.")
                    output_file = None # Fallback to console
                    output_target_name = "stdout (condensed)"
            else:
                output_target_name = "stdout (condensed)"
            print(f"Output target (condensed log): {output_target_name}")


            # --- Processing Loop ---
            header_written_or_printed = False
            if file_format == "csv":
                reader = csv.reader(infile)
                header = next(reader, None)
                lines_processed += 1
                if header:
                    if writer: writer.writerow(header)
                    elif not output_file: print(",".join(header)) # Print header to console if no output file
                    header_written_or_printed = True
                else:
                     print("Warning: CSV file has no header.")

                for row in reader:
                    lines_processed += 1
                    if lines_processed % 50000 == 0: print(".", end='', flush=True)

                    if len(row) < 3: continue # Basic validation

                    current_type = row[0].strip().upper()
                    identifier = None
                    data_tuple = None

                    try:
                        # --- Extract Identifier and Data based on Type ---
                        if current_type == "CAN":
                            identifier_str = row[2].strip() # Column 2 is CAN ID
                            if not identifier_str: continue # Skip if ID is empty
                            identifier = int(identifier_str, 16) # CAN ID as int
                            data_tuple = tuple(item.strip() for item in row[3:]) # DLC and data strings
                        elif current_type == "SERIAL":
                            identifier = "SERIAL"
                            data_tuple = tuple(item.strip() for item in row[2:]) # All remaining columns form the data
                        else:
                            continue # Skip unknown types

                        # --- Apply Filter ---
                        if filter_set and identifier not in filter_set:
                            continue # Skip if identifier not in filter set
                        filtered_messages_count += 1

                        # --- Update Summary Counts ---
                        if identifier not in unique_messages:
                            unique_messages[identifier] = {}
                        if data_tuple not in unique_messages[identifier]:
                            unique_messages[identifier][data_tuple] = 0
                            unique_payload_count += 1 # Count new unique payload within filter
                        unique_messages[identifier][data_tuple] += 1 # Increment count for this payload

                        # --- Condensed Log Output Logic ---
                        last_data = last_data_per_id.get(identifier)
                        if data_tuple != last_data:
                            # Write/Print the changed message to the condensed log output
                            if writer: writer.writerow(row)
                            elif not output_file: print(",".join(row))
                            # Update last seen data *for condensed output*
                            last_data_per_id[identifier] = data_tuple

                    except (ValueError, IndexError) as e:
                        # print(f"\nWarning: Skipping CSV line {lines_processed} due to parsing error ({e}): {row}")
                        continue

            elif file_format == "savvycan_csv":
                reader = csv.reader(infile)
                header = next(reader, None)
                lines_processed += 1
                if header:
                    if writer: writer.writerow(header)
                    elif not output_file: print(",".join(header))
                    header_written_or_printed = True
                else:
                    print("Warning: SavvyCAN CSV file has no header.")

                for row in reader:
                    lines_processed += 1
                    if lines_processed % 50000 == 0: print(".", end='', flush=True)

                    if len(row) < 6: continue # Need at least Time, ID, Ext, Dir, Bus, LEN

                    identifier = None
                    data_tuple = None

                    try:
                        identifier_str = row[1].strip() # Column 1 is ID
                        if not identifier_str: continue
                        identifier = int(identifier_str, 16) # CAN ID as int

                        # --- Apply Filter ---
                        if filter_set and identifier not in filter_set:
                            continue
                        filtered_messages_count += 1

                        dlc_str = row[5].strip() # Column 5 is LEN (DLC)
                        data_strings = [item.strip() for item in row[6:14]]
                        data_tuple = (dlc_str,) + tuple(data_strings)

                        # --- Update Summary Counts ---
                        if identifier not in unique_messages:
                            unique_messages[identifier] = {}
                        if data_tuple not in unique_messages[identifier]:
                            unique_messages[identifier][data_tuple] = 0
                            unique_payload_count += 1
                        unique_messages[identifier][data_tuple] += 1

                        # --- Condensed Log Output Logic ---
                        last_data = last_data_per_id.get(identifier)
                        if data_tuple != last_data:
                            if writer: writer.writerow(row)
                            elif not output_file: print(",".join(row))
                            last_data_per_id[identifier] = data_tuple

                    except (ValueError, IndexError) as e:
                        # print(f"\nWarning: Skipping SavvyCAN line {lines_processed} due to parsing error ({e}): {row}")
                        continue


            elif file_format == "candump":
                # No header for candump format
                for line in infile:
                    lines_processed += 1
                    if lines_processed % 50000 == 0: print(".", end='', flush=True)

                    match = candump_regex.match(line.strip())
                    if not match:
                        continue # Skip lines not matching the format

                    try:
                        can_id_str, data_hex_str = match.groups()
                        identifier = int(can_id_str, 16) # CAN ID as int

                        # --- Apply Filter ---
                        if filter_set and identifier not in filter_set:
                            continue
                        filtered_messages_count += 1

                        if data_hex_str:
                            data_bytes = bytes.fromhex(data_hex_str)
                        else:
                            data_bytes = b'' # Empty data
                        data_tuple = (data_bytes,) # Store bytes object in a tuple

                        # --- Update Summary Counts ---
                        if identifier not in unique_messages:
                            unique_messages[identifier] = {}
                        if data_tuple not in unique_messages[identifier]:
                            unique_messages[identifier][data_tuple] = 0
                            unique_payload_count += 1
                        unique_messages[identifier][data_tuple] += 1

                        # --- Condensed Log Output Logic ---
                        last_data = last_data_per_id.get(identifier)
                        if data_tuple != last_data:
                            if outfile: outfile.write(line) # Write raw line if outputting
                            elif not output_file: print(line.strip())
                            last_data_per_id[identifier] = data_tuple

                    except (ValueError, IndexError) as e:
                        # print(f"\nWarning: Skipping candump line {lines_processed} due to parsing error ({e}): {line.strip()}")
                        continue

        # --- Print Final Summary ---
        print("\n--- Analysis Summary ---", file=sys.stderr)
        print(f"Total lines processed: {total_messages}", file=sys.stderr)
        print(f"Total messages processed after filter: {filtered_messages_count}", file=sys.stderr)
        print(f"Total unique message payloads found (within filter): {unique_payload_count}", file=sys.stderr) # Renamed for clarity

        print("\nUnique Messages Summary (Counts within Filter):", file=sys.stderr)
        # Sort by identifier first for consistent output
        for identifier, data_dict in sorted(unique_messages.items(), key=lambda item: str(item[0])):
            id_str = f"0x{identifier:X}" if isinstance(identifier, int) else identifier
            print(f"  Identifier: {id_str}", file=sys.stderr)
            # Sort unique data payloads for consistent output within each identifier
            for data_tuple, count in sorted(data_dict.items(), key=lambda item: str(item[0])):
                 # Format data nicely for printing
                 if isinstance(identifier, int): # CAN
                     dlc_str = "N/A"
                     data_str = ""
                     if data_tuple and isinstance(data_tuple[0], bytes): # Candump format: tuple of bytes
                         data_bytes_obj = data_tuple[0]
                         data_str = data_bytes_obj.hex(' ').upper()
                         dlc_str = str(len(data_bytes_obj)) # Calculate DLC from bytes length
                     elif data_tuple and len(data_tuple) > 0: # CSV/SavvyCAN format: tuple of strings (DLC, D1, ...)
                         dlc_str = data_tuple[0] # First element is DLC string
                         # Join the data byte STRINGS (index 1 onwards)
                         data_str = " ".join(filter(None, data_tuple[1:])).upper()
                     else: # Handle empty data tuple or unexpected format
                          data_str = ""
                          dlc_str = "0" # Assume 0 if no data
                     print(f"    DLC: {dlc_str}, Data: {data_str}, Count: {count}", file=sys.stderr)
                 else: # SERIAL (or other non-CAN identifier)
                     data_str = ", ".join(map(str, data_tuple))
                     print(f"    Data: [{data_str}], Count: {count}", file=sys.stderr)


    except FileNotFoundError:
        print(f"Error: Input file not found: {log_file}", file=sys.stderr)
    except Exception as e:
        print(f"\nAn unexpected error occurred: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc() # Print full traceback for debugging
    finally:
        if outfile:
            outfile.close()
            print(f"\nAnalysis complete. Condensed log saved to {output_file}" if output_file else "\nAnalysis complete.")


def main():
    parser = argparse.ArgumentParser(
        description="Analyze CAN/Serial log files (auto-detect format) for unique messages, filtering by ID(s).",
        formatter_class=argparse.RawTextHelpFormatter # Keep formatting in help
        )
    parser.add_argument("log_file", help="Path to the input CAN/Serial log file.")
    parser.add_argument("-o", "--output", help="Path to the output file for the *condensed* log (optional, prints condensed log to console if omitted).")
    # --- Updated Filter Argument ---
    parser.add_argument(
        "-f", "--filter",
        nargs='+', # Accept one or more filter arguments
        help='One or more identifiers (CAN ID hex e.g., 0x1A1 or "SERIAL", case-insensitive) to filter for.\nIf omitted, all messages are processed.'
    )
    # --- End Update ---

    args = parser.parse_args()

    # --- Process Filter List ---
    filter_set = set()
    filter_print_list = []
    if args.filter:
        for item in args.filter:
            item_upper = item.upper()
            if item_upper == "SERIAL":
                filter_set.add("SERIAL")
                filter_print_list.append("SERIAL")
            else:
                try:
                    can_id_int = int(item, 16)
                    filter_set.add(can_id_int)
                    filter_print_list.append(f"0x{can_id_int:X}")
                except ValueError:
                    print(f"Error: Invalid CAN ID filter value '{item}'. Must be hexadecimal (e.g., 0x1A1).", file=sys.stderr)
                    sys.exit(1)
    # --- End Filter Processing ---

    print(f"Starting analysis for {args.log_file}...")
    if filter_set:
        print(f"Filtering for Identifier(s): {', '.join(filter_print_list)}")
    else:
        print("No filter applied.")

    # Pass the set (or empty set if no filter) to the analysis function
    analyze_can_log(args.log_file, args.output, filter_set if filter_set else None)

if __name__ == "__main__":
    main()