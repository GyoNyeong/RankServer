# RankServer

MySql,Unity클라이언트와 통신하는 TCPServer

<br>

## 🗂️ **프로젝트 개요**

Unity클라이언트와 통신하여 데이터를 MySql DB에 저장하거나 DB에 저장된 데이터를 클라이언트로 전송

<br>

---
<br>

## 작업 내용
  1. 클라이언트와 통신할 때 RapidJson 이용

  2. 클라이언트로 받은 json을 파싱. 값을 뽑아내 DB에 Insert하는 쿼리 실행

  3. 클라이언트에서 요청을 받으면 "SELECT PlayerName, Score FROM ranking ORDER BY Score DESC LIMIT 10(DB를 Score기준으로 내림차순 정렬해 상위 10개만 전달)" 쿼리 실행

  4. 위에서 받은 데이터를 json배열로 만들어 클라이언트에 전달  
 
       
