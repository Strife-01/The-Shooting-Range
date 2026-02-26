sudo docker-compose up -d db
sudo docker cp ./db_setup.sql shooting_db:/db_setup.sql

./init_db_once.sh

sudo docker exec -it shooting_db psql -U admin -d the_shooting_range_db -c "\dt"

sudo docker-compose up -d
