## Tasks
### 20.08.2026

1. 
# Arithmetic combinations
df['price_per_sqft'] = df['price'] / df['sqft']

# Date/time decomposition
df['year'] = df['date'].dt.year
df['month'] = df['date'].dt.month
df['day_of_week'] = df['date'].dt.dayofweek

# Binning continuous variables
df['age_group'] = pd.cut(df['age'], bins=[0, 18, 35, 60, 100],
                          labels=['minor', 'young_adult', 'adult', 'senior'])

# Interaction terms
df['x1_x2'] = df['x1'] * df['x2']

# Aggregations (e.g., group-level stats)
df['avg_price_by_city'] = df.groupby('city')['price'].transform('mean')

# Text-based features
df['name_length'] = df['name'].str.len()

