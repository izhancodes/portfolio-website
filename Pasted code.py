import pandas as pd

# ================= CONFIG =================
CSV_FILE = "pscproject.csv"
CAR_PLATE_COL = "CAR NUMBER PLATE DETAILS"

# Load dataset
df = pd.read_csv(CSV_FILE)

print("===== Ride Information System =====")

while True:
    print("\nMAIN MENU")
    print("1. View Trip Details")
    print("2. Edit Existing Trip")
    print("3. Add New Trip")
    print("4. Exit")
    print("5. Summarize Trips")

    main_choice = input("Enter your choice (1-5): ").strip()

    # ================= VIEW TRIP =================
    if main_choice == "1":
        trip_id = input("\nEnter Trip ID: ").strip()
        trip = df[df["TRIP-ID."] == trip_id]

        if trip.empty:
            print("Trip ID not found.")
            continue

        while True:
            print("\nVIEW OPTIONS")
            print("1. Start Location")
            print("2. End Location")
            print("3. Driver ID")
            print("4. Customer ID")
            print("5. Total Pay")
            print("6. Car Brand")
            print("7. Car Number Plate Details")
            print("8. Show Complete Details")
            print("9. Back to Main Menu")

            choice = input("Enter choice (1-9): ").strip()

            if choice == "1":
                print("Start Location:", trip["START LOCATION"].values[0])

            elif choice == "2":
                print("End Location:", trip["END LOCATION"].values[0])

            elif choice == "3":
                print("Driver ID:", trip["DRIVER-ID"].values[0])

            elif choice == "4":
                print("Customer ID:", trip["CUSTOMER-ID"].values[0])

            elif choice == "5":
                print("Total Pay:", trip["TOTAL PAY"].values[0])

            elif choice == "6":
                if "CAR BRAND" in trip.columns:
                    print("Car Brand:", trip["CAR BRAND"].values[0])
                else:
                    print("Car Brand: Not Available")

            elif choice == "7":
                if CAR_PLATE_COL in trip.columns:
                    print("Car Number Plate:", trip[CAR_PLATE_COL].values[0])
                else:
                    print("Car Number Plate: Not Available")

            elif choice == "8":
                print("\n📋 COMPLETE DETAILS")
                print(trip)

            elif choice == "9":
                break

            else:
                print("Invalid choice")

    # ================= EDIT TRIP =================
    elif main_choice == "2":
        trip_id = input("\nEnter Trip ID to edit: ").strip()
        index = df[df["TRIP-ID."] == trip_id].index

        if index.empty:
            print("Trip ID not found.")
            continue

        idx = index[0]

        print("\nEDIT OPTIONS")
        print("1. Start Location")
        print("2. End Location")
        print("3. Total Pay")
        print("4. Driver Rating")
        print("5. Car Brand")
        print("6. Car Number Plate Details")

        edit_choice = input("Enter field number to edit: ").strip()
        new_value = input("Enter new value: ")

        if edit_choice == "1":
            df.at[idx, "START LOCATION"] = new_value
        elif edit_choice == "2":
            df.at[idx, "END LOCATION"] = new_value
        elif edit_choice == "3":
            df.at[idx, "TOTAL PAY"] = new_value
        elif edit_choice == "4":
            df.at[idx, "DRIVER RATING"] = new_value
        elif edit_choice == "5":
            if "CAR BRAND" not in df.columns:
                df["CAR BRAND"] = ""
            df.at[idx, "CAR BRAND"] = new_value
        elif edit_choice == "6":
            if CAR_PLATE_COL not in df.columns:
                df[CAR_PLATE_COL] = ""
            df.at[idx, CAR_PLATE_COL] = new_value
        else:
            print("Invalid choice")
            continue

        df.to_csv(CSV_FILE, index=False)
        print("Trip updated successfully")

    # ================= ADD NEW TRIP =================
    elif main_choice == "3":
        print("\nADD NEW TRIP")

        new_trip = {
            "TRIP-ID.": input("Trip ID: "),
            "DATE.": input("Date: "),
            "DRIVER-ID": input("Driver ID: "),
            "CUSTOMER-ID": input("Customer ID: "),
            "START LOCATION": input("Start Location: "),
            "END LOCATION": input("End Location: "),
            "DISTANCE in (KM)": input("Distance (KM): "),
            "TOTAL DURATION.": input("Total Duration: "),
            "START TIME": input("Start Time: "),
            "END TIME": input("End Time: "),
            "TOTAL PAY": input("Total Pay: "),
            "PAYMENT MODE": input("Payment Mode: "),
            "DRIVER RATING": input("Driver Rating: "),
            "RIDE CANCELLED": input("Ride Cancelled (YES/NO): "),
            "CAR BRAND": input("Car Brand: "),
            CAR_PLATE_COL: input("Car Number Plate Details: ")
        }

        df = pd.concat([df, pd.DataFrame([new_trip])], ignore_index=True)
        df.to_csv(CSV_FILE, index=False)
        print("New trip added successfully")

        # ================= SUMMARIZE =================
    elif main_choice == "5":
        print("\n📊 SUMMARY REPORT")

        # -------- Averages --------
        if "TOTAL PAY" in df.columns:
            print("Average Total Pay:", df["TOTAL PAY"].astype(float).mean())

        if "DISTANCE in (KM)" in df.columns:
            print("Average Distance (KM):", df["DISTANCE in (KM)"].astype(float).mean())

        if "DRIVER RATING" in df.columns:
            print("Average Driver Rating:", df["DRIVER RATING"].astype(float).mean())

        # -------- Locations --------
        if "START LOCATION" in df.columns:
            print("Most Started Location:",
                  df["START LOCATION"].value_counts().idxmax())

        if "END LOCATION" in df.columns:
            print("Most Visited Location:",
                  df["END LOCATION"].value_counts().idxmax())

        # -------- Vehicle --------
        if "CAR BRAND" in df.columns:
            print("Most Used Car Brand:",
                  df["CAR BRAND"].value_counts().idxmax())

        if CAR_PLATE_COL in df.columns:
            print("Most Used Car Number Plate:",
                  df[CAR_PLATE_COL].value_counts().idxmax())

        # -------- Driver & Customer --------
        if "DRIVER-ID" in df.columns:
            print("Driver with Maximum Trips:",
                  df["DRIVER-ID"].value_counts().idxmax())

        if "CUSTOMER-ID" in df.columns:
            print("Customer with Maximum Trips:",
                  df["CUSTOMER-ID"].value_counts().idxmax())

        # -------- Ride Status --------
        if "RIDE CANCELLED" in df.columns:
            cancelled = (df["RIDE CANCELLED"] == "YES").sum()
            completed = (df["RIDE CANCELLED"] == "NO").sum()
            print("Total Cancelled Rides:", cancelled)
            print("Total Completed Rides:", completed)

        # -------- Payment --------
        if "PAYMENT MODE" in df.columns:
            print("Most Used Payment Mode:",
                  df["PAYMENT MODE"].value_counts().idxmax())

    # ================= EXIT =================
    elif main_choice == "4":
        print("Thank you for using Ride Information System.")
        break

    else:
        print("Invalid main menu choice")
