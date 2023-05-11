from typing import Union

from sqlalchemy import create_engine
import pandas as pd

class DB_Client:

    def __init__(self, cred):
        self.client = create_engine(f'postgresql+psycopg2://{cred["user"]}:{cred["password"]}@{cred["host"]}:5432/{cred["db_name"]}')
        self.db_connection = self.client.connect()

    def sql_query_db(self, sql_query: str, index_col: Union[str, list]=None):
        """
              Uses pandas method.
              see documentation: https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.read_sql_query.html"
        """
        return pd.read_sql_query(sql=sql_query, con=self.db_connection, index_col=None)

    def insert_data_to_db_table_from_df(self, df: pd.DataFrame, table: str, schema: str, if_exists='append', index=False, method='multi' ):
        """
        Uses pandas method.
        see documentation: https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.to_sql.html"
        """
        return df.to_sql(name=table, con=self.db_connection, schema=schema, if_exists=if_exists, index=index, method=method)
