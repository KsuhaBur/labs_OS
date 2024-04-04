import datetime

from flask import Flask, jsonify, request
import sqlite3
import logging

app = Flask(__name__)
port = 3000

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def connect_db():
    return sqlite3.connect('temperature_data_database.db')

def add_temperature_data(timestamp, temperature):
    try:
        conn = connect_db()
        cursor = conn.cursor()
        query = 'INSERT INTO total_temp (timestamp, temperature) VALUES (?, ?);'
        cursor.execute(query, (timestamp, temperature))
        conn.commit()
        conn.close()
        return True
    except sqlite3.Error as e:
        logger.error('Error adding temperature data:', exc_info=True)
        return False

@app.route('/addTemperatureData', methods=['POST'])
def add_temperature_data_route():
    try:
        data = request.get_json()
        if data is None:
            raise ValueError("No JSON data received")

        timestamp = data.get('timestamp')
        temperature = data.get('temperature')

        if not timestamp or not temperature:
            raise ValueError("Incomplete JSON data")

        if add_temperature_data(timestamp, temperature):
            return 'Data added successfully', 200
        else:
            return 'Failed to add data', 500
    except ValueError as ve:
        logger.error(f'Error parsing request data: {ve}')
        return 'Bad Request: ' + str(ve), 400
    except Exception as e:
        logger.error('Error parsing request data:', exc_info=True)
        return 'Bad Request', 400

@app.route('/clearOldEntries', methods=['POST'])
def clear_old_entries():
    try:
        data = request.get_json()
        table_name = data.get('tableName')
        threshold_timestamp = data.get('threshold')


        threshold_time = datetime.datetime.strptime(threshold_timestamp, '%Y-%m-%d %H:%M:%S')
        threshold_time_sqlite = threshold_time.strftime('%Y-%m-%d %H:%M:%S')


        delete_query = f"DELETE FROM {table_name} WHERE timestamp < '{threshold_time_sqlite}';"

        conn = connect_db()
        cursor = conn.cursor()
        cursor.execute(delete_query)
        conn.commit()
        conn.close()

        return jsonify({'message': 'Old entries cleared successfully'}), 200
    except Exception as e:
        logger.error('Failed to clear old entries in the database:', exc_info=True)
        return jsonify({'error': 'Internal Server Error'}), 500


def get_all_data():
    try:
        conn = connect_db()
        cursor = conn.cursor()
        query_all = 'SELECT * FROM total_temp;'
        query_hour = 'SELECT * FROM hour_temp;'
        query_day = 'SELECT * FROM day_temp;'
        cursor.execute(query_all)
        result_all = cursor.fetchall()
        cursor.execute(query_hour)
        result_hour = cursor.fetchall()
        cursor.execute(query_day)
        result_day = cursor.fetchall()

        responseData = {
            'total_temp': result_all,
            'hour_temp': result_hour,
            'day_temp': result_day,
        }
        conn.close()
        return responseData
    except sqlite3.Error as e:
        logger.error('Error executing queries:', exc_info=True)
        return None

def get_current_temperature():
    try:
        conn = connect_db()
        cursor = conn.cursor()
        current_temperature_query = 'SELECT temperature FROM total_temp ORDER BY timestamp DESC LIMIT 1;'
        cursor.execute(current_temperature_query)
        current_temperature_result = cursor.fetchone()
        if not current_temperature_result:
            return None

        current_temperature = current_temperature_result[0]

        responseData = {
            'currentTemperature': current_temperature,
        }
        conn.close()
        return responseData
    except sqlite3.Error as e:
        logger.error('Error executing queries:', exc_info=True)
        return None

def get_temperature_data(selected_date):
    try:
        conn = connect_db()
        cursor = conn.cursor()
        temperature_data_query = '''
            SELECT id, timestamp, temperature
            FROM total_temp 
            WHERE timestamp >= ? AND timestamp < ?;
        '''
        cursor.execute(temperature_data_query, (selected_date + ' 00:00:00', selected_date + ' 23:59:59'))
        temperature_data_result = cursor.fetchall()

        temperature_data = [{
            'id': entry[0],
            'timestamp': entry[1],
            'temperature': entry[2],
        } for entry in temperature_data_result]

        responseData = {
            'temperatureData': temperature_data
        }
        conn.close()
        return responseData
    except sqlite3.Error as e:
        logger.error('Error executing queries:', exc_info=True)
        return None

def get_period_temperature_data(selected_start_date, selected_end_date):
    try:
        conn = connect_db()
        cursor = conn.cursor()
        temperature_data_query = '''
            SELECT id, timestamp, temperature
            FROM total_temp 
            WHERE timestamp >= ? AND timestamp < ?;
        '''
        cursor.execute(temperature_data_query, (selected_start_date, selected_end_date))
        temperature_data_result = cursor.fetchall()

        temperature_data = [{
            'id': entry[0],
            'timestamp': entry[1],
            'temperature': entry[2],
        } for entry in temperature_data_result]


        responseData = {
            'temperatureData': temperature_data,
        }
        conn.close()
        return responseData
    except sqlite3.Error as e:
        logger.error('Error executing queries:', exc_info=True)
        return None

@app.route('/getAllData')
def get_all_data_route():
    responseData = get_all_data()
    if responseData is not None:
        return jsonify(responseData)
    else:
        return 'Internal Server Error', 500

@app.route('/getCurrentTemperature')
def get_current_temperature_route():
    responseData = get_current_temperature()
    if responseData is not None:
        return jsonify(responseData)
    else:
        return 'Internal Server Error', 500

@app.route('/getTemperatureData')
def get_temperature_data_route():
    selected_date = request.args.get('selectedDate')
    if selected_date:
        responseData = get_temperature_data(selected_date)
        if responseData is not None:
            return jsonify(responseData)
    return 'Bad Request', 400


if __name__ == '__main__':
    connect_db()
    app.run(port=port)
